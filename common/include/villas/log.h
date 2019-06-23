/** Logging and debugging routines
 *
 * @file
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

#pragma once

#include <cstdarg>

#include <jansson.h>

/* The log level which is passed as first argument to print() */
enum log_level {
	LOG_LVL_DEBUG,
	LOG_LVL_INFO,
	LOG_LVL_WARN,
	LOG_LVL_ERROR
};

/** Debug facilities.
 *
 * To be or-ed with the debug level
 */
enum log_facilities {
	LOG_POOL =		(1L <<  8),
	LOG_QUEUE =		(1L <<  9),
	LOG_CONFIG =		(1L << 10),
	LOG_HOOK =		(1L << 11),
	LOG_PATH =		(1L << 12),
	LOG_NODE =		(1L << 13),
	LOG_MEM =		(1L << 14),
	LOG_WEB =		(1L << 15),
	LOG_API =		(1L << 16),
	LOG_LOG =		(1L << 17),
	LOG_VFIO =		(1L << 18),
	LOG_PCI =		(1L << 19),
	LOG_XIL =		(1L << 20),
	LOG_TC =		(1L << 21),
	LOG_IF =		(1L << 22),
	LOG_ADVIO =		(1L << 23),
	LOG_IO =		(1L << 24),

	/* Node-types */
	LOG_SOCKET =		(1L << 25),
	LOG_FILE =		(1L << 26),
	LOG_FPGA =		(1L << 27),
	LOG_NGSI =		(1L << 28),
	LOG_WEBSOCKET =		(1L << 29),
	LOG_OPAL =		(1L << 30),
	LOG_COMEDI =		(1L << 31),
	LOG_IB =		(1LL << 32),

	/* Classes */
	LOG_NODES = LOG_NODE | LOG_SOCKET | LOG_FILE | LOG_FPGA | LOG_NGSI | LOG_WEBSOCKET | LOG_OPAL | LOG_COMEDI | LOG_IB,
	LOG_KERNEL = LOG_VFIO | LOG_PCI | LOG_TC | LOG_IF,
	LOG_ALL = ~0xFF
};

int log_get_width();

/** Printf alike debug message with level. */
void debug(long long lvl, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

/** Printf alike info message. */
void info(const char *fmt, ...)
	__attribute__ ((format(printf, 1, 2)));

/** Printf alike warning message. */
void warning(const char *fmt, ...)
	__attribute__ ((format(printf, 1, 2)));

/** Print error and exit. */
void error(const char *fmt, ...)
	__attribute__ ((format(printf, 1, 2)));

/** Print error and strerror(errno). */
void serror(const char *fmt, ...)
	__attribute__ ((format(printf, 1, 2)));

/** Print configuration error and exit. */
void jerror(json_error_t *err, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));
