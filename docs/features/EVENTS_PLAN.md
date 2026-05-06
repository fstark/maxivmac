# Event Queue Refactor — Implementation Plan

Design: [EVENTS.md](EVENTS.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | TimedEvent struct + EventQ API (header only, compiles) | done |
| 2 | EventQ implementation with unit tests | done |
| 3 | Wire producers to new API | done |
| 4 | Wire consumers to new API | done |
| 5 | Remove old ring buffer code | done |
| 6 | Golden tests re-record + full verification | done |

Build gate: `cmake --preset macos && cmake --build --preset macos`
Test gate:  `./bld/macos/tests && cd test && ./verify.sh`

---

## Phase 1 — TimedEvent struct + EventQ API declaration

Introduce the new data types and function declarations.  No
implementation yet — just enough for the build to succeed with the
new header included.

### 1.1 — Create `src/platform/common/event_queue.h`

New header declaring the timed event queue interface.

```cpp
/*
    event_queue.h — Time-aware event queue for host input delivery.

    Replaces the fixed 16-slot EvtQEl ring buffer with a
    ScaledCycleCount-gated queue.  Events are delivered when the
    emulated clock reaches their fireCycle timestamp.

    See docs/features/EVENTS.md for design rationale.
*/

#pragma once

#include <cstdint>
#include <deque>
#include <span>
#include "core/ict_scheduler.h"   // ScaledCycleCount
#include "platform/platform.h"    // EvtQElKind

/// A single input event with a delivery timestamp.
struct TimedEvent
{
    ScaledCycleCount fireCycle;  ///< Deliver when g_ict.nextCount >= this
    EvtQElKind kind;
    union
    {
        struct { uint8_t key; bool down; } press;
        struct { int16_t h, v; } pos;
    } u;
};

/// Push an event for delivery at a specific cycle.
/// Future events are appended after all immediate events.
void EventQ_Push(TimedEvent evt);

/// Push a key event for immediate delivery (fireCycle = now).
void EventQ_PushKey(uint8_t key, bool down);

/// Push a mouse button event for immediate delivery.
void EventQ_PushButton(bool down);

/// Push an absolute mouse position for immediate delivery.
/// Coalesces with the previous mouse-pos event if one exists.
void EventQ_PushMousePos(int16_t h, int16_t v);

/// Push a mouse delta for immediate delivery.
/// Coalesces with the previous mouse-delta event if one exists.
void EventQ_PushMouseDelta(int16_t dh, int16_t dv);

/// Peek the next deliverable event (fireCycle <= now), or nullptr.
/// Does not consume the event — call EventQ_Pop() after processing.
TimedEvent *EventQ_Peek(ScaledCycleCount now);

/// Pop the front event (must have been confirmed via Peek first).
void EventQ_Pop();

/// Remove all future key events (fireCycle > now).
/// Used by "clear keys" to cancel pending scripted input.
void EventQ_ClearFutureKeys();

/// Return a view over pending future key events (fireCycle > now).
/// Used by "info keys" debugger command.
std::span<const TimedEvent> EventQ_PendingKeys();

/// Return the number of events currently in the queue.
size_t EventQ_Size();

/// Return true if the queue is empty.
bool EventQ_Empty();
```

### 1.2 — Add stub `src/platform/common/event_queue.cpp`

Minimal translation unit that compiles but is not yet called:

```cpp
/*
    event_queue.cpp — Time-aware event queue implementation.

    See event_queue.h for API documentation.
    See docs/features/EVENTS.md for design rationale.
*/

#include "platform/common/event_queue.h"
#include "core/ict_scheduler.h"  // g_ict

// placeholder — implemented in Phase 2
```

### 1.3 — Add to CMakeLists.txt

Add `src/platform/common/event_queue.cpp` to the `maxivmac` target
sources (next to `osglu_common.cpp`).

### Fence

- [ ] `src/platform/common/event_queue.h` exists with full API
- [ ] `src/platform/common/event_queue.cpp` exists (stub)
- [ ] CMakeLists.txt updated
- [ ] Full build clean: `cmake --build --preset macos`
- [ ] Existing tests pass: `./bld/macos/tests`
- [ ] Commit: `"events: phase 1 — TimedEvent struct and EventQ API declaration"`

---

## Phase 2 — EventQ implementation with unit tests

Implement the queue logic and prove it correct with unit tests
before touching any existing code.

### 2.1 — Implement queue storage in `event_queue.cpp`

```cpp
#include "platform/common/event_queue.h"
#include "core/ict_scheduler.h"

/// The event queue.  Partitioned: immediate events (fireCycle <= now)
/// at the front, future events (fireCycle > now) at the back.
/// Within each partition, events are in insertion order.
static std::deque<TimedEvent> s_queue;

/// Temporary staging buffer for EventQ_PendingKeys() span return.
static std::vector<TimedEvent> s_pendingKeysCache;
```

### 2.2 — Implement `EventQ_Push`

Insert future events in sorted order at the back partition.
Immediate events (fireCycle <= g_ict.nextCount) go to the front.

```cpp
void EventQ_Push(TimedEvent evt)
{
    if (evt.fireCycle <= g_ict.nextCount)
    {
        // Immediate: insert before any future events.
        // Find the first future event and insert before it.
        auto it = s_queue.begin();
        while (it != s_queue.end() && it->fireCycle <= g_ict.nextCount)
            ++it;
        s_queue.insert(it, evt);
    }
    else
    {
        // Future: append in sorted order among future events.
        // Future events come from a single source with monotonically
        // increasing timestamps, so appending at the end is O(1)
        // in the expected case.
        auto it = s_queue.end();
        while (it != s_queue.begin())
        {
            auto prev = std::prev(it);
            if (prev->fireCycle <= evt.fireCycle)
                break;
            it = prev;
        }
        s_queue.insert(it, evt);
    }
}
```

### 2.3 — Implement typed push helpers

Each helper constructs a `TimedEvent` with `fireCycle = g_ict.nextCount`
and calls `EventQ_Push`, with coalescing for mouse events:

```cpp
void EventQ_PushKey(uint8_t key, bool down)
{
    TimedEvent evt{};
    evt.fireCycle = g_ict.nextCount;
    evt.kind = EvtQElKind::Key;
    evt.u.press.key = key;
    evt.u.press.down = down;
    EventQ_Push(evt);
}

void EventQ_PushButton(bool down)
{
    TimedEvent evt{};
    evt.fireCycle = g_ict.nextCount;
    evt.kind = EvtQElKind::MouseButton;
    evt.u.press.down = down;
    EventQ_Push(evt);
}

void EventQ_PushMouseDelta(int16_t dh, int16_t dv)
{
    // Coalesce: if the last immediate event is also a MouseDelta, merge.
    ScaledCycleCount now = g_ict.nextCount;
    if (!s_queue.empty())
    {
        // Search backwards from partition boundary for last immediate MouseDelta
        for (auto it = s_queue.begin(); it != s_queue.end() && it->fireCycle <= now; ++it)
        {
            // Track last immediate mouse delta
        }
        // Simpler: check the event just before the future partition
        auto it = s_queue.begin();
        TimedEvent *last = nullptr;
        while (it != s_queue.end() && it->fireCycle <= now)
        {
            if (it->kind == EvtQElKind::MouseDelta)
                last = &(*it);
            ++it;
        }
        if (last)
        {
            last->u.pos.h += dh;
            last->u.pos.v += dv;
            return;
        }
    }
    TimedEvent evt{};
    evt.fireCycle = now;
    evt.kind = EvtQElKind::MouseDelta;
    evt.u.pos.h = dh;
    evt.u.pos.v = dv;
    EventQ_Push(evt);
}

void EventQ_PushMousePos(int16_t h, int16_t v)
{
    // Coalesce: if the last immediate event is also a MousePos, replace.
    ScaledCycleCount now = g_ict.nextCount;
    if (!s_queue.empty())
    {
        TimedEvent *last = nullptr;
        for (auto it = s_queue.begin(); it != s_queue.end() && it->fireCycle <= now; ++it)
        {
            if (it->kind == EvtQElKind::MousePos)
                last = &(*it);
        }
        if (last)
        {
            last->u.pos.h = h;
            last->u.pos.v = v;
            return;
        }
    }
    TimedEvent evt{};
    evt.fireCycle = now;
    evt.kind = EvtQElKind::MousePos;
    evt.u.pos.h = h;
    evt.u.pos.v = v;
    EventQ_Push(evt);
}
```

### 2.4 — Implement `EventQ_Peek` and `EventQ_Pop`

```cpp
TimedEvent *EventQ_Peek(ScaledCycleCount now)
{
    if (s_queue.empty())
        return nullptr;
    if (s_queue.front().fireCycle > now)
        return nullptr;
    return &s_queue.front();
}

void EventQ_Pop()
{
    if (!s_queue.empty())
        s_queue.pop_front();
}
```

### 2.5 — Implement `EventQ_ClearFutureKeys` and `EventQ_PendingKeys`

```cpp
void EventQ_ClearFutureKeys()
{
    ScaledCycleCount now = g_ict.nextCount;
    std::erase_if(s_queue, [now](const TimedEvent &e) {
        return e.fireCycle > now && e.kind == EvtQElKind::Key;
    });
}

std::span<const TimedEvent> EventQ_PendingKeys()
{
    ScaledCycleCount now = g_ict.nextCount;
    s_pendingKeysCache.clear();
    for (const auto &e : s_queue)
    {
        if (e.fireCycle > now && e.kind == EvtQElKind::Key)
            s_pendingKeysCache.push_back(e);
    }
    return s_pendingKeysCache;
}

size_t EventQ_Size() { return s_queue.size(); }
bool EventQ_Empty() { return s_queue.empty(); }
```

### 2.6 — Unit tests: `test/test_event_queue.cpp`

Create a dedicated test file exercising the queue in isolation.
The test file provides its own `g_ict` stub to control `nextCount`.

```cpp
/*
    test_event_queue.cpp — Unit tests for the timed event queue.
*/

#include <doctest/doctest.h>
#include "platform/common/event_queue.h"
#include "core/ict_scheduler.h"

// Stub: we control g_ict.nextCount directly in tests.
ICTScheduler g_ict;
```

Test cases to implement:

| Test Case | What it verifies |
|-----------|-----------------|
| `push_key_immediate` | PushKey at time=0 is immediately peekable |
| `push_key_future_not_visible` | Push with fireCycle=100, peek at time=50 returns nullptr |
| `push_key_future_becomes_visible` | Push fireCycle=100, peek at time=100 returns it |
| `pop_removes_front` | After pop, next peek returns next event or nullptr |
| `ordering_immediate_before_future` | Two events: one now, one future; peek returns now first |
| `multiple_future_sorted` | Push fireCycle=200, then fireCycle=100; peek at 100 returns the 100 event |
| `mouse_delta_coalescing` | Two consecutive PushMouseDelta calls merge into one event |
| `mouse_delta_no_coalesce_after_key` | Push key then delta — no merge, two events |
| `mouse_pos_coalescing` | Two consecutive PushMousePos replace previous coordinates |
| `clear_future_keys` | Push 3 future keys + 1 immediate; ClearFutureKeys removes only future |
| `pending_keys_span` | Push 2 future keys + 1 immediate key; PendingKeys returns only the 2 |
| `empty_queue` | Peek on empty returns nullptr, Size=0, Empty=true |
| `large_queue_no_overflow` | Push 100 events — no crash (proves no 16-slot limit) |
| `button_event` | PushButton produces MouseButton kind |
| `mixed_event_types` | Push key, button, delta, pos; all visible in order |

### 2.7 — Add test file to CMakeLists.txt

Add `test/test_event_queue.cpp` and `src/platform/common/event_queue.cpp`
to the `tests` target.  May also need `src/core/ict_scheduler.cpp` stub
or the test's own `g_ict` definition.

### Fence

- [ ] `event_queue.cpp` fully implemented (all 9 functions)
- [ ] `test/test_event_queue.cpp` compiles and all 15 test cases pass
- [ ] `./bld/macos/tests --test-case="*event*"` — all green
- [ ] Full build clean
- [ ] Commit: `"events: phase 2 — EventQ implementation with unit tests"`

---

## Phase 3 — Wire producers to new API

Replace the 4 producer functions (`Keyboard_UpdateKeyMap`,
`MouseButtonSet`, `MousePositionSet`, `MousePositionSetDelta`) to
push into the new queue instead of the old ring buffer.

Both old and new code will coexist briefly: old ring buffer still
exists (consumers still read from it), but producers now go through
the new path.  To keep the build working, the old producers will
internally call the new API AND the old API simultaneously during
this phase (dual-write).  This ensures consumers still function
until Phase 4 transitions them.

### 3.1 — Add `#include "platform/common/event_queue.h"` to `osglu_common.cpp`

### 3.2 — Modify `Keyboard_UpdateKeyMap()`

After the existing `EvtQElAlloc` logic, also call:
```cpp
EventQ_PushKey(k, down);
```

Keep the `g_theKeys[]` bitmap update — it's consumed directly by
keyboard device code independently of the event queue.

### 3.3 — Modify `MouseButtonSet()`

After existing logic, also call:
```cpp
EventQ_PushButton(down);
```

### 3.4 — Modify `MousePositionSetDelta()`

After existing logic, also call:
```cpp
EventQ_PushMouseDelta(dh, dv);
```

### 3.5 — Modify `MousePositionSet()`

After existing logic, also call:
```cpp
EventQ_PushMousePos(h, v);
```

### Fence

- [ ] All 4 producers dual-write to both old and new queue
- [ ] Full build clean
- [ ] `./bld/macos/tests` — all pass
- [ ] `./test/verify.sh` — golden tests still pass (consumers read old queue)
- [ ] Boot Mac Plus, Mac II — keyboard and mouse work normally
- [ ] Commit: `"events: phase 3 — producers dual-write to old + new queue"`

---

## Phase 4 — Wire consumers to new API

Switch all 9 consumer call sites from `EvtQOutP()`/`EvtQOutDone()`
to `EventQ_Peek(g_ict.nextCount)`/`EventQ_Pop()`.

After this phase, the old ring buffer is dead code (still present
but never read).

### 4.1 — Modify `FindKeyEvent()` in `src/core/machine.cpp`

```cpp
#include "platform/common/event_queue.h"

bool FindKeyEvent(int *VirtualKey, bool *KeyDown)
{
    if (0 != g_masterEvtQLock)
        return false;

    TimedEvent *p = EventQ_Peek(g_ict.nextCount);
    if (p && p->kind == EvtQElKind::Key)
    {
        *VirtualKey = p->u.press.key;
        *KeyDown = p->u.press.down;
        EventQ_Pop();
        return true;
    }
    return false;
}
```

### 4.2 — Modify `MouseDevice::update()` in `src/devices/mouse.cpp`

Replace all 3 `EvtQOutP()` calls with `EventQ_Peek(g_ict.nextCount)`
and `EvtQOutDone()` with `EventQ_Pop()`.

The structure remains identical — the if-chains checking event kind
are preserved.  Only the peek/pop function names change.

Add `#include "platform/common/event_queue.h"` to the file.

### 4.3 — Modify `ADB_DoMouseTalk()` in `src/devices/adb_shared.h`

Replace the two `EvtQOutP()` calls (lines ~38 and ~80) with
`EventQ_Peek(g_ict.nextCount)` and `EvtQOutDone()` with
`EventQ_Pop()`.

### 4.4 — Modify `CheckForADBanyEvt()` in `src/devices/adb_shared.h`

Replace `EvtQOutP()` with `EventQ_Peek(g_ict.nextCount)`.
No pop — this is an existence check only.

### 4.5 — Remove dual-write from producers

Go back to `osglu_common.cpp` and remove the old `EvtQElAlloc` /
`EvtQElPreviousIn` paths from all 4 producer functions, leaving
only the `EventQ_Push*` calls.

### Fence

- [ ] All consumers use `EventQ_Peek`/`EventQ_Pop`
- [ ] Producers only write to new queue (no dual-write)
- [ ] Full build clean
- [ ] `./bld/macos/tests` — all pass
- [ ] `./test/verify.sh` — golden tests still pass
- [ ] Boot Mac Plus, Mac SE, Mac II — full keyboard + mouse validation
- [ ] Commit: `"events: phase 4 — consumers switched to EventQ, remove dual-write"`

---

## Phase 5 — Remove old ring buffer code

Dead code removal.  The old queue infrastructure is now unused.

### 5.1 — Remove from `src/platform/common/osglu_common.cpp`

Delete:
- `g_evtQA[EVT_Q_SZ]` array
- `g_evtQIn`, `g_evtQOut` variables
- `g_evtQNeedRecover` flag
- `EvtQOutP()` function
- `EvtQOutDone()` function
- `EvtQElPreviousIn()` function
- `EvtQElAlloc()` function
- `EvtQTryRecoverFromFull()` function

### 5.2 — Remove from `src/platform/common/osglu_common.h`

Delete:
- `EVT_Q_LG2_SZ`, `EVT_Q_SZ`, `EVT_Q_IMASK` defines
- `extern EvtQEl g_evtQA[]` declaration
- `extern uint16_t g_evtQIn`, `g_evtQOut` declarations
- `extern bool g_evtQNeedRecover` declaration

### 5.3 — Remove `EvtQEl` from `src/platform/platform.h`

Delete the `EvtQEl` struct definition.  Keep `EvtQElKind` — it's
reused by `TimedEvent`.

### 5.4 — Remove recovery call in `src/platform/emulator_shell.cpp`

Find the `EvtQTryRecoverFromFull()` call site (around line 483)
and delete it (or the entire if-block guarded by
`g_evtQNeedRecover`).

### 5.5 — Remove any remaining references

Grep for `EvtQEl `, `EvtQOutP`, `EvtQOutDone`, `EvtQElAlloc`,
`EvtQElPreviousIn`, `g_evtQA`, `g_evtQIn`, `g_evtQOut`,
`g_evtQNeedRecover`, `EVT_Q_SZ`, `EVT_Q_IMASK` across entire
codebase and remove or update as needed (documentation comments,
etc.).

### Fence

- [ ] No reference to old ring buffer remains in `src/`
- [ ] `grep -r "EvtQEl\b\|EvtQOutP\|EvtQElAlloc\|EVT_Q_SZ" src/` — zero hits
- [ ] Full build clean
- [ ] `./bld/macos/tests` — all pass
- [ ] Commit: `"events: phase 5 — remove old ring buffer and recovery hack"`

---

## Phase 6 — Golden tests re-record + full verification

The event queue change is internal plumbing — guest-visible
behaviour should be identical.  But the input delivery timing
may shift by one tick in edge cases (previously immediate, now
cycle-gated), which could affect golden files.

### 6.1 — Run verify.sh and check for regressions

```sh
./test/verify.sh
```

If all 6 models pass, no re-record needed.  Skip to 6.3.

### 6.2 — Re-record golden files (if needed)

If any model diverges:
1. Boot the failing model manually, verify correct behaviour
2. If behaviour is correct: `./test/record.sh <model>`
3. Diff the new golden vs old to confirm changes are trivially
   different (shifted by 1-2 ticks, not corrupted output)

### 6.3 — Add event queue to golden test commentary

Add a note to `test/README.md` explaining that golden tests
implicitly validate event queue delivery (keyboard/mouse input
during boot affects state recorder output).

### 6.4 — Manual testing checklist

Perform and document these manual tests:

| Model | Test | Expected |
|-------|------|----------|
| Mac Plus | Type in MacWrite | Characters appear correctly |
| Mac Plus | Move mouse to menu bar, click | Menu opens |
| Mac SE | Type rapidly (100+ WPM) | No dropped characters |
| Mac II | Click and drag in MacPaint | Continuous stroke |
| Mac II | Modifier keys (Shift, Cmd) | Correctly modify input |
| Classic | Boot to desktop | No assertion, no hang |

### 6.5 — Document the refactor

Add file-level comment to `event_queue.h` and `event_queue.cpp`
noting:
- Design doc: `docs/features/EVENTS.md`
- This replaced the old 16-slot ring buffer
- Future scripting usage via `EventQ_Push` with future timestamps

### Fence

- [ ] `./test/verify.sh` — all 6 models pass (re-recorded if needed)
- [ ] `./bld/macos/tests` — all pass including event queue tests
- [ ] Manual testing performed on at least Mac Plus + Mac II
- [ ] `test/README.md` updated with event queue note
- [ ] Commit: `"events: phase 6 — re-record golden files, manual test pass"`

---

## Summary

6 phases, each independently compilable and testable:

1. **Header** — types + API declaration (no behaviour change)
2. **Implementation + unit tests** — 15 test cases prove queue logic
3. **Producers dual-write** — safe bridge, old consumers still work
4. **Consumers switch** — new queue is active, old queue dead
5. **Dead code removal** — clean slate
6. **Golden re-record + validation** — prove non-regression

Total new test surface:
- 15 unit test cases in `test/test_event_queue.cpp`
- 6 golden model tests (re-recorded if drift detected)
- Manual test checklist for interactive validation

Files created:
- `src/platform/common/event_queue.h`
- `src/platform/common/event_queue.cpp`
- `test/test_event_queue.cpp`

Files modified:
- `CMakeLists.txt` (2 additions: source + test)
- `src/platform/common/osglu_common.cpp` (producer rewrite, dead code removal)
- `src/platform/common/osglu_common.h` (dead code removal)
- `src/platform/platform.h` (remove `EvtQEl` struct)
- `src/platform/emulator_shell.cpp` (remove recovery call)
- `src/core/machine.cpp` (consumer rewrite)
- `src/devices/mouse.cpp` (consumer rewrite)
- `src/devices/adb_shared.h` (consumer rewrite)
- `test/README.md` (documentation)
