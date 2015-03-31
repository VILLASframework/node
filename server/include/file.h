/** Node type: File
 *
 * This file implements the file type for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _FILE_H_
#define _FILE_H_

#include <stdio.h>

#include "node.h"

struct file {
	FILE *in;
	FILE *out;

	const char *path_in;
	const char *path_out;

	const char *mode;	/**< The mode for fopen() which is used for the out file. */
	
	double rate;		/**< The sending rate. */
	int tfd;		/**< Timer file descriptor. Blocks until 1/rate seconds are elapsed. */
};

int file_print(struct node *n, char *buf, int len);

int file_parse(config_setting_t *cfg, struct node *n);

int file_open(struct node *n);

int file_close(struct node *n);

int file_read(struct node *n, struct msg *m);

int file_write(struct node *n, struct msg *m);

#endif /* _FILE_H_ */