/** Node type: Wrapper around RSCAD CBuilder model
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Steffen Vogel
 **********************************************************************************/
 
#ifndef _CBUILDER_H_
#define _CBUILDER_H_

#include <pthread.h>

#include "list.h"

/* Helper macros for registering new models */
#define REGISTER_CBMODEL(cb)				\
__attribute__((constructor)) static void __register() {	\
	list_push(&cbmodels, cb);			\
}

extern struct list cbmodels;	/**< Table of existing CBuilder models */

struct cbuilder {
	unsigned long step, read;
	double timestep;

	struct cbmodel *model;
	
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

struct cbmodel {
	char *name;

	void (*code)();
	void (*ram)();
	
	int (*init)(struct cbuilder *cb);
	int (*read)(float inputs[], int len);
	int (*write)(float outputs[], int len);
};

#endif /* _CBUILDER_H_ */