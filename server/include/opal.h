/** Node type: OPAL (AsyncApi)
 *
 * This file implements the opal subtype for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _OPAL_H_
#define _OPAL_H_

/* Define RTLAB before including OpalPrint.h for messages to be sent
 * to the OpalDisplay. Otherwise stdout will be used. */
#define RTLAB
#include "OpalPrint.h"
#include "AsyncApi.h"

/* This is just for initializing the shared memory access to communicate
 * with the RT-LAB model. It's easier to remember the arguments like this */
#define	OPAL_ASYNC_SHMEM_NAME	argv[1]
#define OPAL_ASYNC_SHMEM_SIZE	atoi(argv[2])
#define OPAL_PRINT_SHMEM_NAME	argv[3]

struct opal {
	Opal_GenAsyncParam_Ctrl icon_ctrl;

	char * async_shmem_name;
	char * print_shmem_name;
	int async_shmem_size;
};

int opal_parse(int argc, char *argv[], struct node *n);

int opal_print(struct node *n, char *buf, int len);

int opal_open(struct node *n);

int opal_close(struct node *n);

int opal_read(struct node *n, struct msg *m);

int opal_write(struct node *n, struct msg *m);

#endif /* _OPAL_H_ */
