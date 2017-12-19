#include "log.hpp"
#include "utils.h"

int Logger::depth;
int Logger::depthCurrent;
Logger::LogLevel Logger::global_level = Logger::LogLevel::Info;

Logger cpp_debug(Logger::LogLevel::Debug,	"" CLR_BLU(" Debug ") "| ");
Logger cpp_info(Logger::LogLevel::Info);
Logger cpp_warn(Logger::LogLevel::Warning,	"" CLR_YEL("Warning") "| ");
Logger cpp_error(Logger::LogLevel::Error,	"" CLR_RED(" Error ") "| ");

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
