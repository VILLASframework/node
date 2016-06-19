/** Node type: Wrapper around RSCAD CBuilder model
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include "node.h"
 
/* Constants from RSCAD */
#define PI        3.1415926535897932384626433832795   // definition of PI
#define TWOPI     6.283185307179586476925286766559    // definition of 2.0*PI
#define E         2.71828182845904523536028747135266  // definition of E
#define EINV      0.36787944117144232159552377016147  // definition of E Inverse (1/E)
#define RT2       1.4142135623730950488016887242097   // definition of square root 2.0
#define RT3       1.7320508075688772935274463415059   // definition of square root 3.0
#define INV_ROOT2 0.70710678118654752440084436210485

double TimeStep;

/* Add your inputs and outputs here */
double IntfIn;
double IntfOut;

/* Add your parameters here */
double R2; // Resistor [Ohm] in SS2
double C2; // Capacitance [F] in SS2
 
#include "cbuilder/static.h"
#include "cbuilder/ram_functions.h"

static double getTimeStep() {
	return TimeStep;
}

int cbuilder_open(struct node *n)
{
	/* Initialize parameters */
	R2 = 1;		/**< R2 = 1 Ohm */
	C2 = 0.001;	/**< C2 = 1000 uF */

	TimeStep = 50e-6;

	#include "cbuilder/ram.h"

	return 0;
}

int cbuilder_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct sample *smp = smps[0];

	smp->values[0].f = IntfOut;

	return 1;
}

int cbuilder_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct sample *smp = smps[0];

	/* Update inputs */
	IntfIn = smp->values[0].f;

	/* Start calculation of 1 step */
	#include "cbuilder/code.h"

	return 1;
}

static struct node_type vt = {
	.name		= "cbuilder",
	.description	= "RTDS CBuilder model",
	.vectorize	= 1,
	.size		= 0,
	.open		= cbuilder_open,
	.read		= cbuilder_read,
	.write		= cbuilder_write,
};

REGISTER_NODE_TYPE(&vt)
