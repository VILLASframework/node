/** Logging utilities for unit test.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Steffen Vogel
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

#include <cstdarg>
#include <memory>

#include <criterion/logging.h>
#include <criterion/options.h>

#include <villas/log.h>
#include <villas/log.hpp>
#include <spdlog/spdlog.h>

extern "C" {
	/* We override criterions function here */
	void criterion_log_noformat(enum criterion_severity severity, const char *msg);
	void criterion_plog(enum criterion_logging_level level, const struct criterion_prefix_data *prefix, const char *msg, ...);
	void criterion_vlog(enum criterion_logging_level level, const char *msg, va_list args);
}

static const char *color_reset = "\e[0m";

struct criterion_prefix_data {
    const char *prefix;
    const char *color;
};

static int format_msg(char *buf, size_t buflen, const char *msg, va_list args)
{
	int len = vsnprintf(buf, buflen, msg, args);

	/* Strip new line */
	char *nl = strchr(buf, '\n');
	if (nl)
		*nl = 0;

	return len;
}

void criterion_log_noformat(enum criterion_severity severity, const char *msg)
{
	auto logger = loggerGetOrCreate("criterion");

	switch (severity) {
		case CR_LOG_WARNING:
			logger->warn(msg);
			break;

		case CR_LOG_ERROR:
			logger->error(msg);
			break;

		case CR_LOG_INFO:
		default:
			logger->info(msg);
			break;	}
}

void criterion_vlog(enum criterion_logging_level /* level */, const char *msg, va_list args)
{
	char formatted_msg[1024];

	//if (level < criterion_options.logging_threshold)
	//	return;

	format_msg(formatted_msg, sizeof(formatted_msg), msg, args);

	auto logger = loggerGetOrCreate("tests");
	logger->info(formatted_msg);
}

void criterion_plog(enum criterion_logging_level /* level */, const struct criterion_prefix_data *prefix, const char *msg, ...)
{
	char formatted_msg[1024];

	va_list args;

	//if (level < criterion_options.logging_threshold)
	//	return;

	va_start(args, msg);
	format_msg(formatted_msg, sizeof(formatted_msg), msg, args);
	va_end(args);

	auto logger = loggerGetOrCreate("tests");

	if      (!strcmp(prefix->prefix, "----") && !strcmp(prefix->color, "\33[0;34m"))
		logger->info(formatted_msg);
	else if (!strcmp(prefix->prefix, "----") && !strcmp(prefix->color,  "\33[1;30m"))
		logger->debug(formatted_msg);
	else if (!strcmp(prefix->prefix, "===="))
		logger->info(formatted_msg);
	else if (!strcmp(prefix->prefix, "WARN") || strstr(formatted_msg, "Warning"))
		logger->warn(formatted_msg);
	else if (!strcmp(prefix->prefix, "ERR ") || strstr(formatted_msg, "Failed"))
		logger->error(formatted_msg);
	else if (!strcmp(prefix->prefix, "RUN "))
		logger->info("{}Run:{} {}", prefix->color, color_reset, formatted_msg);
	else if (!strcmp(prefix->prefix, "SKIP"))
		logger->info("{}Skip:{} {}", prefix->color, color_reset, formatted_msg);
	else if (!strcmp(prefix->prefix, "PASS"))
		logger->info("{}Pass:{} {}", prefix->color, color_reset, formatted_msg);
	else if (!strcmp(prefix->prefix, "FAIL"))
		logger->error("{}Fail:{} {}", prefix->color, color_reset, formatted_msg);
	else
		logger->info(formatted_msg);
}

extern "C"
void log_cb(struct log *l, enum log_level lvl, const char *fmt, va_list args)
{
	char formatted_msg[1024];

	auto logger = loggerGetOrCreate(l->name);

	//if (level < criterion_options.logging_threshold)
	//	return;

	format_msg(formatted_msg, sizeof(formatted_msg), fmt, args);

	switch (lvl) {
		case LOG_LVL_DEBUG:
			logger->debug(formatted_msg);
			break;

		case LOG_LVL_INFO:
			logger->info(formatted_msg);
			break;

		case LOG_LVL_WARN:
			logger->warn(formatted_msg);
			break;

		case LOG_LVL_ERROR:
			logger->error(formatted_msg);
			break;

		case LOG_LVL_STATS:
			logger->info("Stats: {}", formatted_msg);
			break;
	}
}

void init_logging()
{
	log_set_callback(global_log, log_cb);

	spdlog::set_level(spdlog::level::trace);
	spdlog::set_pattern("[%T] [%^%l%$] [%n] %v");
}