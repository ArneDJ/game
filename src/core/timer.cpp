#include <chrono>
#include <array>

#include "timer.h"

using namespace CORE;

void Timer::begin(void)
{
	ticks = std::chrono::steady_clock::now();
}

void Timer::end(void)
{
	auto timenow = std::chrono::steady_clock::now();
	std::chrono::duration<float> duration = timenow - ticks;

	delta = duration.count();
	elapsed += delta;

	tick_count++;
	if (tick_count % 10 == 0) {
		ms_per_frame = uint32_t(1000 * delta);
	}
}
