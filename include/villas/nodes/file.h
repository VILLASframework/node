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

#include "advio.h"
#include "node.h"

#define FILE_MAX_PATHLEN	512

enum {
	FILE_READ,
	FILE_WRITE
};

struct file {
	struct file_direction {
		AFILE *handle;		/**< libc: stdio file handle. */

		const char *mode;	/**< libc: fopen() mode. */
		const char *fmt;	/**< Format string for file name. */

		char *uri;		/**< Real file name. */
	} read, write;

	enum read_epoch_mode {
		FILE_EPOCH_DIRECT,
		FILE_EPOCH_WAIT,
		FILE_EPOCH_RELATIVE,
		FILE_EPOCH_ABSOLUTE,
		FILE_EPOCH_ORIGINAL
	} read_epoch_mode;		/**< Specifies how file::offset is calculated. */

	struct timespec read_first;	/**< The first timestamp in the file file::{read,write}::uri */
	struct timespec read_epoch;	/**< The epoch timestamp from the configuration. */
	struct timespec read_offset;	/**< An offset between the timestamp in the input file and the current time */
	
	enum {
		FILE_EOF_EXIT,		/**< Terminate when EOF is reached. */
		FILE_EOF_REWIND,	/**< Rewind the file when EOF is reached. */
		FILE_EOF_WAIT		/**< Blocking wait when EOF is reached. */
	} read_eof;			/**< Should we rewind the file when we reach EOF? */
	int read_timer;			/**< Timer file descriptor. Blocks until 1 / rate seconds are elapsed. */
	double read_rate;		/**< The read rate. */
};

/** @see node_type::print */
char * file_print(struct node *n);

/** @see node_type::parse */
int file_parse(struct node *n, config_setting_t *cfg);

/** @see node_type::open */
int file_start(struct node *n);

/** @see node_type::close */
int file_stop(struct node *n);

/** @see node_type::read */
int file_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::write */
int file_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */
