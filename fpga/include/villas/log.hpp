#pragma once

#include <iostream>
#include <string>

#define _ESCAPE	"\x1b"
#define TXT_BOLD(s) _ESCAPE "[1m" + std::string(s) + _ESCAPE "[0m"

class LoggerIndent;

class Logger {
    friend LoggerIndent;
public:

	enum class LogLevel : int {
		Debug,
		Info,
		Warning,
		Error,
		Disabled
	};

    class LoggerNewline {
    public:
        LoggerNewline(bool enabled = true) : enabled(enabled) {}
        ~LoggerNewline() {
            if(enabled)
                std::cout << std::endl;
        }
        template <typename T>
        LoggerNewline& operator<< (T const& value) {
            if(enabled)
                std::cout << value;
            return *this;
        }
        bool enabled;
    };

    class Indenter {
    public:
        Indenter(Logger* l) : logger(l)
        { logger->increaseIndention(); }

        ~Indenter()
        { logger->decreaseIndention(); }
    private:
        Logger* logger;
    };

	Logger(LogLevel level, std::string prefix = "") : level(level), prefix(prefix) {}

    Indenter indent()
    { return Indenter(this); }

    static std::string
    getPadding()
    {
        std::string out = "";
        for(int i = 0; i < depthCurrent; i++) out.append("\u2551 ");
        return out;
    }

    template <typename T>
    LoggerNewline operator<< (T const& value) {
        if(level >= global_level) {

            if(depth > depthCurrent) {
                std::cout << Logger::getPadding() << "\u255f\u2500\u2556" << std::endl;
                depthCurrent++;
            }
            std::cout << Logger::getPadding() << "\u255f " << prefix << value;
            return LoggerNewline();
        } else {
            return LoggerNewline(false);
        }
    }


    void
    increaseIndention()
    {
        depth++;
    }

    void
    decreaseIndention()
    {
        if(depth == depthCurrent)
            std::cout << Logger::getPadding() << std::endl;

        depthCurrent = --depth;
    }

	static
	void
	setLogLevel(LogLevel level)
	{ global_level = level; }

private:
	LogLevel level;
    std::string prefix;
    static int depth;
	static LogLevel global_level;
    static int depthCurrent;
};


extern Logger cpp_debug;
extern Logger cpp_info;
extern Logger cpp_warn;
extern Logger cpp_error;
