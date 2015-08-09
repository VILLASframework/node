/** Node type: File
 *
 * This file implements the file type for nodes.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *
 * @addtogroup file File-IO node type
 * @{
 *********************************************************************************/


#ifndef _FILE_H_
#define _FILE_H_

#include "node.h"

#define FILE_MAX_PATHLEN 128

struct file {
	FILE *in;
	FILE *out;

	char *path_in;
	char *path_out;

	const char *file_mode;	/**< The mode for fopen() which is used for the out file. */

	enum epoch_mode {
		EPOCH_NOW,
		EPOCH_RELATIVE,
		EPOCH_ABSOLUTE
	} epoch_mode;		/**< Specifies how file::offset is calculated. */

	struct timespec start;	/**< The first timestamp of the input file. */
	struct timespec epoch;	/**< The epoch timestamp from the configuration. */
	struct timespec offset;	/**< An offset between the timestamp in the input file and the current time */

	double rate;		/**< The sending rate. */
	int tfd;		/**< Timer file descriptor. Blocks until 1/rate seconds are elapsed. */
};

/** @see node_vtable::init */
int file_init(int argc, char *argv[], struct settings *set);

/** @see node_vtable::deinit */
int file_deinit();

/** @see node_vtable::print */
int file_print(struct node *n, char *buf, int len);

/** @see node_vtable::parse */
int file_parse(config_setting_t *cfg, struct node *n);

/** @see node_vtable::open */
int file_open(struct node *n);

/** @see node_vtable::close */
int file_close(struct node *n);

/** @see node_vtable::read */
int file_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

/** @see node_vtable::write */
int file_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

#endif /** _FILE_H_ @} */
