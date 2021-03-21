#include <fstream>
#include <iostream>

#include "logger.h"

extern void write_log(LogType type, const std::string &message)
{
	std::ofstream error("error.log", std::ios::app);
	std::ofstream runtime("runtime.log", std::ios::app);

	switch (type) {
	case LogType::ERROR:
		error << message.c_str() << std::endl;
		error.close();
		// print to terminal
		std::cout << message.c_str() << std::endl;
		break;
	case LogType::RUN:
		runtime << message.c_str() << std::endl;
		runtime.close();
		// print to terminal
		std::cout << message.c_str() << std::endl;
		break;
	}
}

void write_start_log(void)
{
	write_log(LogType::ERROR, "********** Starting new game (this is not an error) **********\n");
}

void write_exit_log(void)
{
	write_log(LogType::ERROR, "********** Exiting game (this is not an error) **********\n\n");
}
