/** Logging.
 *
 * @file
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#include <spdlog/spdlog.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>

#include <jansson.h>

namespace villas {

/* Forward declarations */
class Log;

using Logger = std::shared_ptr<spdlog::logger>;

extern Log logging;

class Log {

public:
	using Level = spdlog::level::level_enum;
	using DefaultSink = std::shared_ptr<spdlog::sinks::stderr_color_sink_mt>;
	using DistSink = std::shared_ptr<spdlog::sinks::dist_sink_mt>;

protected:
	DistSink sinks;
	DefaultSink sink;

	Level level;

	std::string pattern;		/**< Logging format. */
	std::string prefix;		/**< Prefix each line with this string. */

public:

	Log(Level level = Level::info);

	/**< Get the real usable log output width which fits into one line. */
	int getWidth();

	void parse(json_t *cfg);

	Logger get(const std::string &name);

	void setPattern(const std::string &pattern);
	void setLevel(Level lvl);
	void setLevel(const std::string &lvl);

	Level getLevel() const;
	std::string getLevelName() const;
};

} /* namespace villas */
