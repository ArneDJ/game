
class Timer {
public:
	float delta = 0.f; // time of a frame in seconds
	float elapsed = 0.f;
public:
	void begin(void);
	void end(void);
private:
	std::chrono::time_point<std::chrono::steady_clock> ticks; // ugly syntax
};
