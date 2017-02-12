/** This is c-code for CBuilder component for Subsystem 2
 *   Solver used as in RTDS: Resistive companion (Dommel's algo)
 *   Subsystem 1 is modelled in RSCAD
 *  
 *  % Circuit topology
 *                                                 %
 *  %            *** Subsystem 1 (SS1) ***         %   *** Subsystem 2 (SS2) ***
 *  %                                              %
 *  %             |---------|       |---------|    %
 *  %   |---------|    R1   |-------|    L1   |----%-------|---------|
 *  %   |         |---------|       |---------|    %       |         |
 *  %   |                                          %       |         |
 *  % -----                                        %     -----     -----
 *  % | + |                                        %     |   |     |   |
 *  % | E |                                        %     |C2 |     | R2|
 *  % | - |                                        %     |   |     |   |
 *  % -----                                        %     -----     -----
 *  %   |                                          %       |         |
 *  %   |------------------------------------------%------------------
 *                                                 %
 *                                                 %
 */


/* Variables declared here may be used as parameters inputs or outputs
 * The have to match with whats in Subsystem.h */
#if defined(VILLAS) || SECTION == INPUTS
double IntfIn;
#endif /* defined(VILLAS) || SECTION == INPUTS */

#if defined(VILLAS) || SECTION == OUTPUTS
double IntfOut;
#endif /* defined(VILLAS) || SECTION == OUTPUTS */

#if defined(VILLAS) || SECTION == PARAMS
double R2;			/**< Resistor [Ohm] in SS2 */
double C2;			/**< Capacitance [F] in SS2 */
#endif /* defined(VILLAS) || SECTION == PARAMS */

/* Variables declared here may be used in both the RAM: and CODE: sections below. */
#if defined(VILLAS) || SECTION == STATIC
double dt;
double GR2, GC2;		/**< Inductances of components */
double GnInv;			/**< Inversion of conductance matrix (here only scalar) */
double vC2Hist, iC2Hist, AC2; 	/**< History meas. and current of dynamic elements */
double Jn;			/**< Source vector in equation Gn*e=Jn */
double eSS2;			/**< Node voltage solution */
#endif /* defined(VILLAS) || SECTION == STATIC */


/* This section should contain any 'c' functions
 * to be called from the RAM section (either
 * RAM_PASS1 or RAM_PASS2). Example:
 *
 * static double myFunction(double v1, double v2)
 * {
 *     return(v1*v2);
 * }
 */
#if defined(VILLAS) || SECTION == RAM_FUNCTIONS

/* Nothing here */

#endif /* defined(VILLAS) || SECTION == RAM_FUNCTIONS */


/* Place C code here which computes constants
 * required for the CODE: section below.  The C
 * code here is executed once, prior to the start
 * of the simulation case.
 */
#if defined(VILLAS) || SECTION == RAM

void simple_circuit_ram() {
	GR2 = 1/R2;
	GC2 = 2*C2/dt;		/**< Trapezoidal rule */
	GnInv = 1/(GR2+GC2);	/**< eq. conductance (inverted) */

	vC2Hist =  0.0;		/**< Voltage over C2 in previous time step */
	iC2Hist =  0.0;		/**< Current through C2 in previous time step */
}

#endif /* defined(VILLAS) || SECTION == RAM */


// -----------------------------------------------
// Place C code here which runs on the RTDS. The
// code below is entered once each simulation
// step.
// -----------------------------------------------
#if defined(VILLAS) || SECTION == CODE

void simple_circuit_code() {
	/* Update source vector */
	AC2 = iC2Hist+vC2Hist*GC2;
	Jn = IntfIn+AC2;
	
	/* Solution of the equation Gn*e=Jn; */
	eSS2 = GnInv*Jn;
	
	/* Post step -> calculate the voltage and current for C2 for next step and set interface output */
	vC2Hist= eSS2;
	iC2Hist = vC2Hist*GC2-AC2;
	IntfOut = eSS2;
}

#endif /* defined(VILLAS) || SECTION == CODE */


/* Interface to VILLASnode */
#if defined(VILLAS)

#include <villas/nodes/cbuilder.h>
#include <villas/plugin.h>

double getTimeStep()
{
	return dt;
}

/** Place C code here which intializes parameters */
int simple_circuit_init(struct cbuilder *cb)
{
	if (cb->paramlen < 2)
		return -1; /* not enough parameters given */

	R2 = cb->params[0];
	C2 = cb->params[1];

	/* 'dt' is a special parameter */
	dt = cb->timestep;

	return 0; /* success */
}

/** Place C code here reads model outputs */
int simple_circuit_read(float outputs[], int len)
{
	if (len < 1)
		return -1; /* not enough space */

	outputs[0] = IntfOut;

	return 1; /* 1 value per sample */
}

/** Place C code here which updates model inputs */
int simple_circuit_write(float inputs[], int len)
{
	if (len < 1)
		return -1; /* not enough values */

	IntfIn = inputs[0];

	return 0;
}

static struct plugin p = {
	.name		= "simple_circuit",
	.description	= "A simple CBuilder model",
	.type		= LOADABLE_TYPE_MODEL_CBUILDER,
	.cb 		= {
		.code  = simple_circuit_code,
		.init  = simple_circuit_init,
		.read  = simple_circuit_read,
		.write = simple_circuit_write,
		.ram   = simple_circuit_ram
	}
};

REGISTER_PLUGIN(&p)

#endif /* defined(VILLAS) */