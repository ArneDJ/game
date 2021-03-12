
enum class LogType {
	ERROR,
	RUN
};

extern void write_log(LogType type, const std::string &message);
