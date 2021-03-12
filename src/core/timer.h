
class Timer {
public:
	float delta = 0.0; // time of a frame in seconds
public:
	void begin(void);
	void end(void);
private:
	std::chrono::time_point<std::chrono::steady_clock> ticks; // ugly syntax
};
