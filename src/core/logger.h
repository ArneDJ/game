
enum class LogType {
	LOG_ERROR,
	LOG_RUN
};

extern void write_log(LogType type, const std::string &message);

void write_start_log(void);

void write_exit_log(void);
