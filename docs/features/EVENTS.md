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
`MouseDevice::startTickNotify()` (mouse device code).

None of this is guest-visible.  The queue is purely host-internal
plumbing between the platform input layer and device emulation.

## New design

```cpp
struct TimedEvent
{
    uint64_t fireCycle;        // g_instructionCount when deliverable
    EvtQElKind kind;
    union {
        struct { uint8_t key; bool down; } press;
        struct { int16_t h, v; } pos;
    } u;
};
```

Storage: `std::deque<TimedEvent>` kept sorted by `fireCycle`.
Real-time input pushes with `fireCycle = g_instructionCount`
(immediate).  Scripted input pushes with future timestamps.

Since real-time events are always "now" and scripted events are
always in the future, in practice we're appending immediates at the
front and future events at the back — a deque handles both ends
efficiently.  A full heap isn't needed unless we expect interleaved
future timestamps from multiple sources, which we don't.

### API

```cpp
// Push an event for delivery at a specific cycle
void EventQ_Push(TimedEvent evt);

// Push an event for immediate delivery
void EventQ_PushNow(EvtQElKind kind, ...);

// Peek next deliverable event (fireCycle <= now), or nullptr
TimedEvent *EventQ_Peek(uint64_t now);

// Pop (after peek confirmed delivery)
void EventQ_Pop();

// Remove all future key events (for "clear keys")
void EventQ_ClearFutureKeys();

// Iterate future key events (for "info keys")
std::span<const TimedEvent> EventQ_PendingKeys();
```

### Consumer changes

`FindKeyEvent()` and `MouseDevice::startTickNotify()` change from:
```cpp
if ((p = EvtQOutP()) && p->kind == Key) ...
```
to:
```cpp
if ((p = EventQ_Peek(g_instructionCount)) && p->kind == Key) ...
```

Minimal diff.  Same polling pattern, just adds a time gate.

### Producer changes

`Keyboard_UpdateKeyMap()`, `MouseButtonSet()`, etc. switch from
`EvtQElAlloc()` to `EventQ_PushNow(...)`.  Mechanical replacement.

### Scripting usage (later)

```cpp
// type "hi" → push h↓, h↑, i↓, i↑ with staggered timestamps
uint64_t t = g_instructionCount;
for (auto [keycode, shift] : chars) {
    EventQ_Push({t,         Key, {keycode, true}});
    EventQ_Push({t + 80000, Key, {keycode, false}});
    t += 160000;
}
```

No scheduler.  No ICT task.  No separate pending list.

## Scope

- Replace `g_evtQA[]` ring with `std::deque<TimedEvent>`
- Update all 4 producers (`Keyboard_UpdateKeyMap`, `MouseButtonSet`,
  `MousePositionSet`, `MousePositionSetDelta`)
- Update all consumers (`FindKeyEvent`, `MouseDevice::startTickNotify`,
  ADB code in `adb_shared.h`)
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

- `src/platform/common/osglu_common.h` — new struct + API
- `src/platform/common/osglu_common.cpp` — implementation
- `src/core/machine.cpp` — `FindKeyEvent()` consumer
- `src/devices/mouse.cpp` — mouse consumer
- `src/devices/adb_shared.h` — ADB consumer
- `src/devices/keyboard.cpp` — keyboard consumer
