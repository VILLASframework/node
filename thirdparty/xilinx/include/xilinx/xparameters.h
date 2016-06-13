#ifndef _X_PARAMETERS_H_
#define _X_PARAMETERS_H_

/* We set all instance counters to zero
 * to avoid the generation of configuration tables
 * See: *_g.c, *_sinit.c source files
 */
#define XPAR_XLLFIFO_NUM_INSTANCES 0
#define XPAR_XTMRCTR_NUM_INSTANCES 0
#define XPAR_XAXIDMA_NUM_INSTANCES 0
#define XPAR_XAXIS_SWITCH_NUM_INSTANCES 0

#define XPAR_XINTC_NUM_INSTANCES 0
#define XPAR_INTC_MAX_NUM_INTR_INPUTS 0

#endif