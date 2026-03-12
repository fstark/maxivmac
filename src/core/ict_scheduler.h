/*
	ict_scheduler.h

	Cycle-based task scheduler for the emulated machine.
	Replaces global ICTactive/ICTwhen/NextiCount state.

	Part of Phase 4: Device Interface & Machine Object.
*/

#pragma once
#include <cstdint>
#include <functional>
#include <array>

static constexpr int kMaxICTasks = 16;

using iCountt = uint32_t;

class ICTScheduler {
public:
	using TaskHandler = std::function<void()>;

	void zap();
	void add(int taskId, uint32_t cyclesFromNow);

	iCountt getCurrent() const;
	void    doCurrentTasks();
	int32_t doGetNext(uint32_t maxn) const;

	// Register a handler for a task ID
	void registerTask(int taskId, TaskHandler handler);

	// CPU cycle coupling
	void setCycleAccessors(
		std::function<int32_t()> getCyclesRemaining,
		std::function<void(int32_t)> setCyclesRemaining
	);

	uint32_t active    = 0;
	std::array<iCountt, kMaxICTasks> when{};
	iCountt  nextCount = 0;

private:
	std::array<TaskHandler, kMaxICTasks> handlers_{};
	std::function<int32_t()>      getCyclesRemaining_;
	std::function<void(int32_t)>  setCyclesRemaining_;
};
