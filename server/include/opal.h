/** Node type: OPAL (libOpalAsync API)
 *
 * This file implements the opal subtype for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 * @file
 * @addtogroup opal OPAL-RT Asyc Process node type
 * @{
 */

#ifndef _OPAL_H_
#define _OPAL_H_

#include <pthread.h>

#include "node.h"
#include "msg.h"

/* Define RTLAB before including OpalPrint.h for messages to be sent
 * to the OpalDisplay. Otherwise stdout will be used. */
#define RTLAB
#include "OpalPrint.h"
#include "AsyncApi.h"
#include "OpalGenAsyncParamCtrl.h"

/** This global structure holds libOpalAsync related information.
 *  It's only used once in the code. */
struct opal_global {
	/** Shared Memory identifiers and size, provided via argv. */
	char *async_shmem_name, *print_shmem_name;
	int async_shmem_size;
	
	/** Number of send blocks used in the running OPAL model. */
	int send_icons, recv_icons;
	/** A dynamically allocated array of SendIDs. */
	int *send_ids, *recv_ids;
	
	/** String and Float parameters, provided by the OPAL AsyncProcess block. */
	Opal_GenAsyncParam_Ctrl params;
	
	/** Big Global Lock for libOpalAsync API */
	pthread_mutex_t lock;
};

struct opal {
	int reply;
	int mode;

	int send_id;
	int recv_id;

	int seq_no;
	
	Opal_SendAsyncParam send_params;
	Opal_RecvAsyncParam recv_params;
};

/** Initialize global OPAL settings and maps shared memory regions.
 *
 * @see node_vtable::init
 */
int opal_init(int argc, char *argv[], struct settings *set);

/** Free global OPAL settings and unmaps shared memory regions.
 *
 * @see node_vtable::deinit
 */
int opal_deinit();

/** @see node_vtable::parse */
int opal_parse(config_setting_t *cfg, struct node *n);

/** @see node_vtable::print */
int opal_print(struct node *n, char *buf, int len);

int opal_print_global(struct opal_global *g);

/** @see node_vtable::open */
int opal_open(struct node *n);

/** @see node_vtable::close */
int opal_close(struct node *n);

/** @see node_vtable::read */
int opal_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

/** @see node_vtable::write */
int opal_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

#endif /** _OPAL_H_ @} */
