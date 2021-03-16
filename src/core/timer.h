
class Timer {
public:
	float delta = 0.f; // time of a frame in seconds
	float elapsed = 0.f;
	uint32_t ms_per_frame = 0; // average miliseconds per frame
public:
	void begin(void);
	void end(void);
private:
	std::chrono::time_point<std::chrono::steady_clock> ticks; // ugly syntax
	uint16_t tick_count = 0;
};
