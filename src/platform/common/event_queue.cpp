/*
	event_queue.cpp — Time-aware event queue implementation.

	See event_queue.h for API documentation.
	See docs/features/EVENTS.md for design rationale.
*/

#include "platform/common/event_queue.h"
#include "core/ict_scheduler.h" // g_ict
#include <algorithm>
#include <vector>

/// The event queue.  Partitioned: immediate events (fireCycle <= now)
/// at the front, future events (fireCycle > now) at the back.
/// Within each partition, events are in insertion order.
static std::deque<TimedEvent> s_queue;

/// Staging buffer for EventQ_PendingKeys() span return.
static std::vector<TimedEvent> s_pendingKeysCache;

void EventQ_Push(TimedEvent evt)
{
	if (evt.fireCycle <= g_ict.nextCount)
	{
		// Immediate: insert before any future events.
		auto it = s_queue.begin();
		while (it != s_queue.end() && it->fireCycle <= g_ict.nextCount)
			++it;
		s_queue.insert(it, evt);
	}
	else
	{
		// Future: insert in sorted order among future events.
		auto it = s_queue.end();
		while (it != s_queue.begin())
		{
			auto prev = std::prev(it);
			if (prev->fireCycle <= evt.fireCycle) break;
			it = prev;
		}
		s_queue.insert(it, evt);
	}
}

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
	ScaledCycleCount now = g_ict.nextCount;
	// Coalesce: find the last immediate MouseDelta event and merge.
	TimedEvent *last = nullptr;
	for (auto it = s_queue.begin(); it != s_queue.end() && it->fireCycle <= now; ++it)
	{
		if (it->kind == EvtQElKind::MouseDelta) last = &(*it);
	}
	if (last)
	{
		last->u.pos.h += dh;
		last->u.pos.v += dv;
		return;
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
	ScaledCycleCount now = g_ict.nextCount;
	// Coalesce: find the last immediate MousePos event and replace.
	TimedEvent *last = nullptr;
	for (auto it = s_queue.begin(); it != s_queue.end() && it->fireCycle <= now; ++it)
	{
		if (it->kind == EvtQElKind::MousePos) last = &(*it);
	}
	if (last)
	{
		last->u.pos.h = h;
		last->u.pos.v = v;
		return;
	}
	TimedEvent evt{};
	evt.fireCycle = now;
	evt.kind = EvtQElKind::MousePos;
	evt.u.pos.h = h;
	evt.u.pos.v = v;
	EventQ_Push(evt);
}

TimedEvent *EventQ_Peek(ScaledCycleCount now)
{
	if (s_queue.empty()) return nullptr;
	if (s_queue.front().fireCycle > now) return nullptr;
	return &s_queue.front();
}

void EventQ_Pop()
{
	if (!s_queue.empty()) s_queue.pop_front();
}

void EventQ_ClearFutureKeys()
{
	ScaledCycleCount now = g_ict.nextCount;
	std::erase_if(s_queue, [now](const TimedEvent &e)
				  { return e.fireCycle > now && e.kind == EvtQElKind::Key; });
}

std::span<const TimedEvent> EventQ_PendingKeys()
{
	ScaledCycleCount now = g_ict.nextCount;
	s_pendingKeysCache.clear();
	for (const auto &e : s_queue)
	{
		if (e.fireCycle > now && e.kind == EvtQElKind::Key) s_pendingKeysCache.push_back(e);
	}
	return s_pendingKeysCache;
}

size_t EventQ_Size()
{
	return s_queue.size();
}

bool EventQ_Empty()
{
	return s_queue.empty();
}
