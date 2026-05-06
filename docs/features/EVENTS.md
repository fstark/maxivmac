# Event Queue Refactor

Replace the fixed 16-slot circular event buffer with a time-sorted
event queue.  All input events (key, mouse button, mouse position)
carry a delivery timestamp and are dispatched when the emulated clock
reaches that time.

## Motivation

The scripting feature needs to enqueue future key events with
specific timing (press duration, inter-key gaps).  The current ring
buffer is purely FIFO with no notion of time — events are delivered
immediately when the consumer polls.

Rather than bolt on a separate "script key scheduler" with its own
ICT task, we make the event queue itself time-aware.  Then scripted
key injection is just: push events with future timestamps.  No
separate scheduler, no ICT slot consumed, no second queue.

## Current state

```
EvtQEl g_evtQA[16];          // fixed ring buffer
uint16_t g_evtQIn, g_evtQOut;

EvtQElAlloc()     → push (immediate)
EvtQOutP()        → peek front
EvtQOutDone()     → pop front
```

Producers: `Keyboard_UpdateKeyMap()`, `MouseButtonSet()`,
`MousePositionSet()`, `MousePositionSetDelta()`.

Consumers: `FindKeyEvent()` (keyboard/ADB device code),
`MouseDevice::update()` (mouse device code),
`ADB_DoMouseTalk()` / `CheckForADBanyEvt()` (ADB device code).

None of this is guest-visible.  The queue is purely host-internal
plumbing between the platform input layer and device emulation.

## New design

```cpp
struct TimedEvent
{
    ScaledCycleCount fireCycle; // g_ict.nextCount when deliverable
    EvtQElKind kind;
    union {
        struct { uint8_t key; bool down; } press;
        struct { int16_t h, v; } pos;
    } u;
};
```

The clock source is `ScaledCycleCount` (the ICT scheduler's
cycle counter, `g_ict.nextCount`).  This is the right unit because:
- Consumers run in device-tick context, driven by cycles
- "10 ms" maps directly to cycles at a known clock rate
- `InstructionCount` is per-instruction and has no fixed
  relationship to wall time

See glossary: `ScaledCycleCount`, `kCycleScale`, `InstructionCount`.

Storage: `std::deque<TimedEvent>`, partitioned into two regions:
immediate events (fireCycle ≤ now) at the front, future events at
the back.  Real-time input pushes with `fireCycle = g_ict.nextCount`
(immediate).  Scripted input pushes with future timestamps.

This is a two-partition scheme, not a general sorted queue.  It
works because real-time events are always "now" and scripted events
are monotonically increasing from a single source.  A full heap
isn't needed.

### API

```cpp
// Push an event for delivery at a specific cycle
void EventQ_Push(TimedEvent evt);

// Push key event for immediate delivery
void EventQ_PushKey(uint8_t key, bool down);

// Push mouse button event for immediate delivery
void EventQ_PushButton(bool down);

// Push mouse position for immediate delivery
void EventQ_PushMousePos(int16_t h, int16_t v);

// Push mouse delta for immediate delivery
void EventQ_PushMouseDelta(int16_t dh, int16_t dv);

// Peek next deliverable event (fireCycle <= now), or nullptr
TimedEvent *EventQ_Peek(ScaledCycleCount now);

// Pop (after peek confirmed delivery)
void EventQ_Pop();

// Remove all future key events (for "clear keys")
void EventQ_ClearFutureKeys();

// Iterate future key events (for "info keys")
std::span<const TimedEvent> EventQ_PendingKeys();
```

### Consumer changes

All consumers of `EvtQOutP()` switch to `EventQ_Peek(g_ict.nextCount)`.
Call sites (9 total):

- `FindKeyEvent()` in `machine.cpp` — called from:
  - `keyboard.cpp` (ADB inquiry response)
  - `adb_shared.h` (`CheckForADBkeyEvt`, 2 paths)
- `MouseDevice::update()` in `mouse.cpp` — consumes MouseDelta,
  MousePos, MouseButton (3 call sites)
- `ADB_DoMouseTalk()` in `adb_shared.h` — consumes MouseDelta
- `CheckForADBanyEvt()` in `adb_shared.h` — existence check

Example diff:
```cpp
// before
if ((p = EvtQOutP()) && p->kind == Key) ...
// after
if ((p = EventQ_Peek(g_ict.nextCount)) && p->kind == Key) ...
```

Minimal diff.  Same polling pattern, just adds a time gate.

### Producer changes

Producers switch from `EvtQElAlloc()` to typed push helpers.
Call sites:

- `Keyboard_UpdateKeyMap()` → `EventQ_PushKey()` (18 call sites
  across `emulator_shell.cpp`, `keyboard_map.cpp`, `alt_keys.h`,
  recovery path in `osglu_common.cpp`)
- `MouseButtonSet()` → `EventQ_PushButton()` (5 call sites in
  `emulator_shell.cpp`, `null_keyboard.cpp`, `sdl_keyboard.cpp`,
  recovery path)
- `MousePositionSet()` → `EventQ_PushMousePos()` (1 call site)
- `MousePositionSetDelta()` → `EventQ_PushMouseDelta()` (1 call site)

Mechanical replacement — the existing functions become thin
wrappers or are inlined at call sites.

### Scripting usage (later)

```cpp
// type "hi" → push h↓, h↑, i↓, i↑ with staggered timestamps
ScaledCycleCount t = g_ict.nextCount;
for (auto [keycode, shift] : chars) {
    EventQ_Push({t,         EvtQElKind::Key, {keycode, true}});
    EventQ_Push({t + 80000, EvtQElKind::Key, {keycode, false}});
    t += 160000;
}
```

The gap constants (80000, 160000) are in scaled-cycle units.
At 8 MHz × kCycleScale(64), 160000 scaled cycles ≈ 312 µs.
Tune to taste — real key presses are ~50–100 ms.

No scheduler.  No ICT task.  No separate pending list.

## Scope

- Replace `g_evtQA[]` ring with `std::deque<TimedEvent>`
- Update all 4 producers (`Keyboard_UpdateKeyMap`, `MouseButtonSet`,
  `MousePositionSet`, `MousePositionSetDelta`)
- Update all consumers (`FindKeyEvent`, `MouseDevice::update`,
  `ADB_DoMouseTalk`, `CheckForADBanyEvt` in `adb_shared.h`)
- Remove `EvtQElAlloc`, `EvtQOutP`, `EvtQOutDone`,
  `EvtQElPreviousIn`, `EvtQTryRecoverFromFull`, `g_evtQNeedRecover`
- Remove `EVT_Q_SZ`, `EVT_Q_LG2_SZ`, `EVT_Q_IMASK` defines
- Keep `g_theKeys[4]` bitmap (consumed by keyboard device directly)
- Keep `g_masterEvtQLock` gating logic (just applied to new API)

## Mouse coalescing

The current code coalesces consecutive mouse deltas/positions
(`EvtQElPreviousIn` check).  Preserve this: when pushing a mouse
move with `fireCycle = now`, check if the back of the immediate
section is also a mouse move and merge.

## Testing

Boot several Mac models, verify:
- Keyboard input works (type in a text editor)
- Mouse tracking works (move cursor, click menus)
- No event loss under rapid input
- `g_evtQNeedRecover` path is gone (queue is unbounded)

This is a prerequisite refactor for scripting but is independently
valuable (removes the 16-event ceiling and the recovery hack).

## Files touched

- `src/platform/common/osglu_common.h` — new struct + API decl
- `src/platform/common/osglu_common.cpp` — queue implementation,
  remove old ring buffer + recovery hack
- `src/platform/platform.h` — `EvtQEl` struct removed (replaced by `TimedEvent`)
- `src/platform/emulator_shell.cpp` — producer call sites (keyboard, mouse)
- `src/platform/common/keyboard_map.cpp` — `Keyboard_updateKeyMap2`
- `src/platform/common/alt_keys.h` — alt-key remapping producers
- `src/platform/null_keyboard.cpp` — mouse button producer
- `src/platform/sdl_keyboard.cpp` — mouse button producer
- `src/core/machine.cpp` — `FindKeyEvent()` consumer
- `src/devices/mouse.cpp` — `MouseDevice::update()` consumer
- `src/devices/adb_shared.h` — ADB consumers
- `src/devices/keyboard.cpp` — keyboard consumer
