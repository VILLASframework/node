/** Node type: File
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <cstdio>

#include <villas/format.hpp>
#include <villas/task.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

#define FILE_MAX_PATHLEN	512

struct file {
	Format *formatter;
	FILE *stream_in;
	FILE *stream_out;

	char *uri_tmpl;			/**< Format string for file name. */
	char *uri;			/**< Real file name. */

	unsigned skip_lines;		/**< Skip the first n-th lines/samples of the file. */
	int flush;			/**< Flush / upload file contents after each write. */
	struct Task task;		/**< Timer file descriptor. Blocks until 1 / rate seconds are elapsed. */
	double rate;			/**< The read rate. */
	size_t buffer_size_out;		/**< Defines size of output stream buffer. No buffer is created if value is set to zero. */
	size_t buffer_size_in;		/**< Defines size of input stream buffer. No buffer is created if value is set to zero. */

	enum class EpochMode {
		DIRECT,
		WAIT,
		RELATIVE,
		ABSOLUTE,
		ORIGINAL
	} epoch_mode;			/**< Specifies how file::offset is calculated. */

	enum class EOFBehaviour {
		STOP,			/**< Terminate when EOF is reached. */
		REWIND,			/**< Rewind the file when EOF is reached. */
		SUSPEND			/**< Blocking wait when EOF is reached. */
	} eof_mode;

	struct timespec first;		/**< The first timestamp in the file file::{read,write}::uri */
	struct timespec epoch;		/**< The epoch timestamp from the configuration. */
	struct timespec offset;		/**< An offset between the timestamp in the input file and the current time */
};

char * file_print(NodeCompat *n);

int file_parse(NodeCompat *n, json_t *json);

int file_start(NodeCompat *n);

int file_stop(NodeCompat *n);

int file_init(NodeCompat *n);

int file_destroy(NodeCompat *n);

int file_poll_fds(NodeCompat *n, int fds[]);

int file_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int file_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} /* namespace node */
} /* namespace villas */
