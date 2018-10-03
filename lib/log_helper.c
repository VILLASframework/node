/** Logging and debugging routines
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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

#include <errno.h>
#include <unistd.h>
#include <syslog.h>

#include <villas/utils.h>
#include <villas/log.h>

void debug(long long class, const char *fmt, ...)
{
	va_list ap;

	struct log *l = global_log;

	int lvl = class &  0xFF;
	int fac = class & ~0xFF;

	if (((fac == 0) || (fac & l->facilities)) && (lvl <= l->level)) {
		va_start(ap, fmt);

		log_vprint(l, LOG_LVL_DEBUG, fmt, ap);

		if (l->syslog)
			syslog(LOG_DEBUG, fmt, ap);

		va_end(ap);
	}
}

void info(const char *fmt, ...)
{
	va_list ap;

	struct log *l = global_log;

	va_start(ap, fmt);

	log_vprint(l, LOG_LVL_INFO, fmt, ap);

	if (l->syslog)
		syslog(LOG_INFO, fmt, ap);

	va_end(ap);
}

void warn(const char *fmt, ...)
{
	va_list ap;

	struct log *l = global_log;

	va_start(ap, fmt);

	log_vprint(l, LOG_LVL_WARN, fmt, ap);

	if (l->syslog)
		syslog(LOG_WARNING, fmt, ap);

	va_end(ap);
}

void stats(const char *fmt, ...)
{
	va_list ap;

	struct log *l = global_log;

	va_start(ap, fmt);

	log_vprint(l, LOG_LVL_STATS, fmt, ap);

	if (l->syslog)
		syslog(LOG_INFO, fmt, ap);

	va_end(ap);
}

void error(const char *fmt, ...)
{
	va_list ap;

	struct log *l = global_log;

	va_start(ap, fmt);

	log_vprint(l, LOG_LVL_ERROR, fmt, ap);

	if (l->syslog)
		syslog(LOG_ERR, fmt, ap);

	va_end(ap);

	killme(SIGABRT);
	pause();
}

void serror(const char *fmt, ...)
{
	va_list ap;
	char *buf = NULL;

	struct log *l = global_log;

	va_start(ap, fmt);
	vstrcatf(&buf, fmt, ap);
	va_end(ap);

	log_print(l, LOG_LVL_ERROR, "%s: %m (%u)", buf, errno);

	if (l->syslog)
		syslog(LOG_ERR, "%s: %m (%u)", buf, errno);

	free(buf);

	killme(SIGABRT);
	pause();
}
