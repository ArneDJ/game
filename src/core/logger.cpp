#include <fstream>
#include <iostream>

#include "logger.h"

/*
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
*/

extern void write_error_log(const std::string &message)
{
	std::ofstream error("error.log", std::ios::app);

	error << message.c_str() << std::endl;
	error.close();
	// print to terminal
	std::cout << message.c_str() << std::endl;
}

extern void write_runtime_log(const std::string &message)
{
	std::ofstream runtime("runtime.log", std::ios::app);

	runtime << message.c_str() << std::endl;
	runtime.close();
	// print to terminal
	std::cout << message.c_str() << std::endl;
}

void write_start_log(void)
{
	write_error_log("********** Starting new game (this is not an error) **********\n");
}

void write_exit_log(void)
{
	write_error_log("********** Exiting game (this is not an error) **********\n\n");
}
