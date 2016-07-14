// This is c-code for CBuilder component for Subsystem 2
// Solver used as in RTDS: Resistive companion (Dommel's algo)
// Subsystem 1 is modelled in RSCAD
//
//% Circuit topology
//                                               %
//%            *** Subsystem 1 (SS1) ***         %   *** Subsystem 2 (SS2) ***
//%                                              %
//%             |---------|       |---------|    %
//%   |---------|    R1   |-------|    L1   |----%-------|---------|
//%   |         |---------|       |---------|    %       |         |          
//%   |                                          %       |         |
//% -----                                        %     -----     -----
//% | + |                                        %     |   |     |   |
//% | E |                                        %     |C2 |     | R2|
//% | - |                                        %     |   |     |   |
//% -----                                        %     -----     -----           
//%   |                                          %       |         |
//%   |------------------------------------------%------------------    
//                                               %
//                                               %  

/* These constants are defined by RTDS in the component header file */
#if defined(VILLAS) || SECTION == CONSTANTS
  #define PI        3.1415926535897932384626433832795   // definition of PI                 
  #define TWOPI     6.283185307179586476925286766559    // definition of 2.0*PI             
  #define E         2.71828182845904523536028747135266  // definition of E                  
  #define EINV      0.36787944117144232159552377016147  // definition of E Inverse (1/E)    
  #define RT2       1.4142135623730950488016887242097   // definition of square root 2.0    
  #define RT3       1.7320508075688772935274463415059   // definition of square root 3.0    
  #define INV_ROOT2 0.70710678118654752440084436210485
#endif

// -----------------------------------------------   
// Variables declared here may be used as parameters
// inputs or outputs
// The have to match with whats in Subsystem.h             
// -----------------------------------------------
#if defined(VILLAS) || SECTION == INPUTS
double IntfIn;
#endif

#if defined(VILLAS) || SECTION == OUTPUTS
double IntfOut;
#endif

#if defined(VILLAS) || SECTION == PARAMETERS
double R2; // Resistor [Ohm] in SS2
double C2; // Capacitance [F] in SS2
#endif

// -----------------------------------------------   
// Variables declared here may be used in both the   
// RAM: and CODE: sections below.                    
// -----------------------------------------------
#if defined(VILLAS) || SECTION == STATIC
double dt; 
double GR2, GC2; //Inductances of components
double GnInv; //Inversion of conductance matrix (here only scalar)
double vC2Hist, iC2Hist, AC2; // history meas. and current of dynamic elements
double Jn; //source vector in equation Gn*e=Jn
double eSS2; //node voltage solution 
#endif

// -----------------------------------------------   
// This section should contain any 'c' functions     
// to be called from the RAM section (either         
// RAM_PASS1 or RAM_PASS2). Example:                 
//                                                   
// static double myFunction(double v1, double v2)    
// {                                                 
//     return(v1*v2);                                
// }                                                 
// -----------------------------------------------   
#if defined(VILLAS) || SECTION == RAM_FUNCTIONS
/* Nothing here */
#endif

// -----------------------------------------------   
// Place C code here which computes constants        
// required for the CODE: section below.  The C      
// code here is executed once, prior to the start    
// of the simulation case.                           
// -----------------------------------------------
#if defined(VILLAS) || SECTION == RAM

void ram() {
	GR2 = 1/R2;  
	GC2 = 2*C2/dt;  //trapezoidal rule
	GnInv = 1/(GR2+GC2); //eq. conductance (inverted)
	
	vC2Hist =  0.0; //Voltage over C2 in previous time step
	iC2Hist =  0.0; //Current through C2 in previous time step
}

#endif

// -----------------------------------------------   
// Place C code here which runs on the RTDS. The     
// code below is entered once each simulation        
// step.                                             
// -----------------------------------------------
#if defined(VILLAS) || SECTION == CODE

void code() {
	//Update source vector
	AC2 = iC2Hist+vC2Hist*GC2;
	Jn = IntfIn+AC2;
	//Solution of the equation Gn*e=Jn;
	eSS2 = GnInv*Jn;  
	//Post step -> calculate the voltage and current for C2 for next step and set interface output
	vC2Hist= eSS2;
	iC2Hist = vC2Hist*GC2-AC2;
	IntfOut = eSS2;
}

#endif

// -----------------------------------------------   
// The following code portion is VILLASnode specific
// -----------------------------------------------   
#if defined(VILLAS)

#include "nodes/cbuilder.h"

// -----------------------------------------------   
// Place C code here which intializes parameters
// -----------------------------------------------   
void init(struct cbuilder *cb)
{
	R2 = cb->params[0];
	C2 = cb->params[1];

	dt = cb->timestep;
}

// -----------------------------------------------   
// Place C code here reads model outputs
// -----------------------------------------------  
int read(float outputs[], int len)
{
	outputs[0] = IntfOut;
}

// -----------------------------------------------   
// Place C code here which updates model inputs
// -----------------------------------------------  
int write(float inputs[], int len)
{
	IntfIn = inputs[0];
}

static struct cbmodel cb = {
	.name = "simple_circuit",
	.code = code,
	.init = init,
	.read = read,
	.write = write,
};

REGISTER_CBMODEL(&cb);

#endif