#include <fstream>
#include <iostream>

#include "logger.h"

extern void write_log(LogType type, const std::string &message)
{
	std::ofstream error("error.txt", std::ios::app);
	std::ofstream runtime("runtime.txt", std::ios::app);

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

