/** Logging and debugging routines
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <unistd.h>
#include <cstdio>
#include <cerrno>

#include <villas/utils.h>
#include <villas/log.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result" 

using namespace villas;

int log_get_width()
{
	return logging.getWidth();
}

void debug(long long, const char *fmt, ...)
{
	va_list ap;

	char *buf;

	va_start(ap, fmt);
	vasprintf(&buf, fmt, ap);
	va_end(ap);

	Logger logger = logging.get("default");

	logger->debug(buf);

	free(buf);
}

void info(const char *fmt, ...)
{
	va_list ap;

	char *buf;

	va_start(ap, fmt);
	vasprintf(&buf, fmt, ap);
	va_end(ap);

	Logger logger = logging.get("default");

	logger->info(buf);

	free(buf);
}

void warning(const char *fmt, ...)
{
	va_list ap;

	char *buf;

	va_start(ap, fmt);
	vasprintf(&buf, fmt, ap);
	va_end(ap);

	Logger logger = logging.get("default");

	logger->warn(buf);

	free(buf);
}

void error(const char *fmt, ...)
{
	va_list ap;

	char *buf;

	va_start(ap, fmt);
	vasprintf(&buf, fmt, ap);
	va_end(ap);

	Logger logger = logging.get("default");

	logger->error(buf);

	free(buf);

	killme(SIGABRT);
	pause();
}

void serror(const char *fmt, ...)
{
	va_list ap;

	char *buf;

	va_start(ap, fmt);
	vasprintf(&buf, fmt, ap);
	va_end(ap);

	Logger logger = logging.get("default");

	logger->error(buf);

	free(buf);

	killme(SIGABRT);
	pause();
}

void jerror(json_error_t *err, const char *fmt, ...)
{
	va_list ap;

	char *buf;

	va_start(ap, fmt);
	vasprintf(&buf, fmt, ap);
	va_end(ap);

	Logger logger = logging.get("default");

	logger->error("{}:", buf);
	logger->error("   {} in {}:{}:{}", err->text, err->source, err->line, err->column);

	free(buf);

	killme(SIGABRT);
	pause();
}

#pragma GCC diagnostic pop
