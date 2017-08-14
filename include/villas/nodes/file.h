/** Node type: File
 *
 * This file implements the file type for nodes.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

/**
 * @addtogroup file File-IO node type
 * @ingroup node
 * @{
 */

#pragma once

#include "io.h"
#include "node.h"
#include "periodic_task.h"

#define FILE_MAX_PATHLEN	512

struct file {
	struct io io;			/**< Format and file IO */
	struct io_format *format;

	char *uri_tmpl;			/**< Format string for file name. */
	char *uri;			/**< Real file name. */
	char *mode;			/**< File access mode. */

	int flush;			/**< Flush / upload file contents after each write. */
	struct periodic_task timer;	/**< Timer file descriptor. Blocks until 1 / rate seconds are elapsed. */
	double rate;			/**< The read rate. */

	enum epoch_mode {
		FILE_EPOCH_DIRECT,
		FILE_EPOCH_WAIT,
		FILE_EPOCH_RELATIVE,
		FILE_EPOCH_ABSOLUTE,
		FILE_EPOCH_ORIGINAL
	} epoch_mode;			/**< Specifies how file::offset is calculated. */
	
	enum {
		FILE_EOF_EXIT,		/**< Terminate when EOF is reached. */
		FILE_EOF_REWIND,	/**< Rewind the file when EOF is reached. */
		FILE_EOF_WAIT		/**< Blocking wait when EOF is reached. */
	} eof;

	struct timespec first;		/**< The first timestamp in the file file::{read,write}::uri */
	struct timespec epoch;		/**< The epoch timestamp from the configuration. */
	struct timespec offset;		/**< An offset between the timestamp in the input file and the current time */
};

/** @see node_type::print */
char * file_print(struct node *n);

/** @see node_type::parse */
int file_parse(struct node *n, json_t *cfg);

/** @see node_type::open */
int file_start(struct node *n);

/** @see node_type::close */
int file_stop(struct node *n);

/** @see node_type::read */
int file_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::write */
int file_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */
