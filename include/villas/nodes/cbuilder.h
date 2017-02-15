/** Node type: Wrapper around RSCAD CBuilder model
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 */
/**
 * @addtogroup cbuilder RTDS CBuilder model node
 * @ingroup node
 * @{
 *********************************************************************************/
 
#ifndef _NODES_CBUILDER_H_
#define _NODES_CBUILDER_H_

#include <pthread.h>

#include "list.h"

/* Forward declaration */
struct cbuilder;

struct cbuilder_model {
	char *name;

	void (*code)();
	void (*ram)();
	
	int (*init)(struct cbuilder *cb);
	int (*read)(float inputs[], int len);
	int (*write)(float outputs[], int len);
};

struct cbuilder {
	unsigned long step, read;
	double timestep;

	struct cbuilder_model *model;
	
	float *params;
	int paramlen;

	/* This mutex and cv are used to protect model parameters, input & outputs
	 *
	 * The cbuilder_read() function will wait for the completion of a simulation step
	 * before returning.
	 * The simulation step is triggerd by a call to cbuilder_write().
	 */
	pthread_mutex_t mtx;
	pthread_cond_t cv;
};

#endif /** _NODES_CBUILDER_H_ @} */