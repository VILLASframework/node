/** Logging.
 *
 * @file
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#pragma once

#include <string>

#define SPDLOG_LEVEL_NAMES { "trace", "debug", "info ",  "warn ", "error", "crit ", "off   " }
#define SPDLOG_NAME_WIDTH 17

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#define _ESCAPE	"\x1b"
#define TXT_RESET_ALL	_ESCAPE "[0m"

#define TXT_RESET_BOLD	_ESCAPE "[21m"
#define TXT_BOLD(s)		_ESCAPE "[1m" + std::string(s)  + TXT_RESET_BOLD

#define TXT_RESET_COLOR	_ESCAPE "[39m"
#define TXT_RED(s)		_ESCAPE "[31m" + std::string(s) + TXT_RESET_COLOR
#define TXT_GREEN(s)	_ESCAPE "[32m" + std::string(s) + TXT_RESET_COLOR
#define TXT_YELLOW(s)	_ESCAPE "[33m" + std::string(s) + TXT_RESET_COLOR
#define TXT_BLUE(s)		_ESCAPE "[34m" + std::string(s) + TXT_RESET_COLOR

using SpdLogger = std::shared_ptr<spdlog::logger>;

inline SpdLogger loggerGetOrCreate(const std::string& logger_name)
{
    auto logger = spdlog::get(logger_name);
    if(not logger) {
        logger = spdlog::stdout_color_mt(logger_name);
    }
    return logger;
}
