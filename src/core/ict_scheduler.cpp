/*
	ict_scheduler.cpp

	ICT (Interrupt/Cycle Timer) scheduler implementation.
	Extracted from machine.cpp globals into a class.

	Part of Phase 4: Device Interface & Machine Object.
*/

#include "core/ict_scheduler.h"

#ifdef _VIA_Debug
#include <cstdio>
#endif

void ICTScheduler::zap()
{
	active = 0;
}

void ICTScheduler::setCycleAccessors(
	std::function<int32_t()> getCyclesRemaining,
	std::function<void(int32_t)> setCyclesRemaining)
{
	getCyclesRemaining_ = std::move(getCyclesRemaining);
	setCyclesRemaining_ = std::move(setCyclesRemaining);
}

void ICTScheduler::registerTask(int taskId, TaskHandler handler)
{
	handlers_[taskId] = std::move(handler);
}

iCountt ICTScheduler::getCurrent() const
{
	return nextCount - getCyclesRemaining_();
}

void ICTScheduler::add(int taskId, uint32_t n)
{
	/* n must be > 0 */
	int32_t x = getCyclesRemaining_();
	uint32_t whenVal = nextCount - x + n;

#ifdef _VIA_Debug
	fprintf(stderr, "ICT_add: %d, %d, %d\n", whenVal, taskId, n);
#endif
	when[taskId] = whenVal;
	active |= (1 << taskId);

	if (x > (int32_t)n) {
		setCyclesRemaining_(n);
		nextCount = whenVal;
	}
}

void ICTScheduler::doCurrentTasks()
{
	int i = 0;
	uint32_t m = active;

	while (0 != m) {
		if (0 != (m & 1)) {
			if (i >= kMaxICTasks) {
				/* shouldn't happen */
				active &= ((1 << kMaxICTasks) - 1);
				m = 0;
			} else if (when[i] == nextCount) {
				active &= ~(1 << i);
#ifdef _VIA_Debug
				fprintf(stderr, "doing task %d, %d\n", nextCount, i);
#endif
				if (handlers_[i]) {
					handlers_[i]();
				}
			}
		}
		++i;
		m >>= 1;
	}
}

int32_t ICTScheduler::doGetNext(uint32_t maxn) const
{
	int i = 0;
	uint32_t m = active;
	uint32_t v = maxn;

	while (0 != m) {
		if (0 != (m & 1)) {
			if (i >= kMaxICTasks) {
				/* shouldn't happen */
				m = 0;
			} else {
				uint32_t d = when[i] - nextCount;
				/* at this point d must be > 0 */
				if (d < v) {
#ifdef _VIA_Debug
					fprintf(stderr, "coming task %d, %d, %d\n",
						nextCount, i, d);
#endif
					v = d;
				}
			}
		}
		++i;
		m >>= 1;
	}

	return v;
}
