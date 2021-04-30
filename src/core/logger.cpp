#include <fstream>
#include <iostream>

#include "logger.h"

extern void write_log(LogType type, const std::string &message)
{
	std::ofstream error("error.log", std::ios::app);
	std::ofstream runtime("runtime.log", std::ios::app);

	switch (type) {
	case LogType::LOG_ERROR:
		error << message.c_str() << std::endl;
		error.close();
		// print to terminal
		std::cout << message.c_str() << std::endl;
		break;
	case LogType::LOG_RUN:
		runtime << message.c_str() << std::endl;
		runtime.close();
		// print to terminal
		std::cout << message.c_str() << std::endl;
		break;
	}
}

void write_start_log(void)
{
	write_log(LogType::LOG_ERROR, "********** Starting new game (this is not an error) **********\n");
}

void write_exit_log(void)
{
	write_log(LogType::LOG_ERROR, "********** Exiting game (this is not an error) **********\n\n");
}
