#pragma once

#include <string>

#define SPDLOG_LEVEL_NAMES { "trace", "debug", "info ",  "warn ", "error", "crit ", "off   " }
#define SPDLOG_NAME_WIDTH 17

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#define _ESCAPE	"\x1b"
#define TXT_BOLD(s) _ESCAPE "[1m" + std::string(s) + _ESCAPE "[0m"

using SpdLogger = std::shared_ptr<spdlog::logger>;

inline SpdLogger loggerGetOrCreate(const std::string& logger_name)
{
    auto logger = spdlog::get(logger_name);
    if(not logger) {
        logger = spdlog::stdout_color_mt(logger_name);
    }
    return logger;
}
