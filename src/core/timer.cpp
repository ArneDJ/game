#include <chrono>

#include "timer.h"

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
}
