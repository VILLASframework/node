#include "log.hpp"
#include "utils.h"

int Logger::depth;
int Logger::depthCurrent;
int Logger::global_level = 0;

Logger cpp_debug(0, CLR_BLU("Debug: "));
Logger cpp_info(20);
Logger cpp_warn(80, CLR_YEL("Warning: "));
Logger cpp_error(100, CLR_RED("Error: "));

void test()
{
	cpp_debug << "Hello";
	{
		Logger::Indenter indent = cpp_debug.indent();
		cpp_debug << "indented";
		{
			Logger::Indenter indent = cpp_debug.indent();
			cpp_debug << "indented";
		}
	}

	cpp_debug << "and root again";
}
