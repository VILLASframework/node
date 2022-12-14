/** Logging.
 *
 * @file
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

#include <string>
#include <list>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/fmt/ostr.h>

#include <jansson.h>

namespace villas {

// Forward declarations
class Log;

using Logger = std::shared_ptr<spdlog::logger>;

extern Log logging;

class Log {

public:
	using Level = spdlog::level::level_enum;
	using DefaultSink = std::shared_ptr<spdlog::sinks::stderr_color_sink_mt>;
	using DistSink = std::shared_ptr<spdlog::sinks::dist_sink_mt>;
	using Formatter = std::shared_ptr<spdlog::pattern_formatter>;

	class Expression {
	public:
		std::string name;
		Level level;

		Expression(const std::string &n, Level lvl) :
			name(n),
			level(lvl)
		{ }

		Expression(json_t *json);
	};

protected:
	DistSink sinks;
	DefaultSink sink;
	Formatter formatter;

	Level level;

	std::string pattern;		/**< Logging format. */
	std::string prefix;		/**< Prefix each line with this string. */

	std::list<Expression> expressions;

public:

	Log(Level level = Level::info);

	/**< Get the real usable log output width which fits into one line. */
	int getWidth();

	void parse(json_t *json);

	Logger get(const std::string &name);

	void setFormatter(const std::string &pattern, const std::string &pfx = "");
	void setLevel(Level lvl);
	void setLevel(const std::string &lvl);

	Level getLevel() const;
	std::string getLevelName() const;
};

} // namespace villas
