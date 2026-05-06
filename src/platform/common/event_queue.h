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
#include "core/ict_scheduler.h" // ScaledCycleCount
#include "platform/platform.h"	// EvtQElKind

/// A single input event with a delivery timestamp.
struct TimedEvent
{
	ScaledCycleCount fireCycle; ///< Deliver when g_ict.nextCount >= this
	EvtQElKind kind;
	union
	{
		struct
		{
			uint8_t key;
			bool down;
		} press;
		struct
		{
			int16_t h, v;
		} pos;
	} u;
};

/// Push an event for delivery at a specific cycle.
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
void EventQ_ClearFutureKeys();

/// Return a view over pending future key events (fireCycle > now).
std::span<const TimedEvent> EventQ_PendingKeys();

/// Return the number of events currently in the queue.
size_t EventQ_Size();

/// Return true if the queue is empty.
bool EventQ_Empty();
