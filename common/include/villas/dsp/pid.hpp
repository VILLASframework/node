/** A PID controller.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

namespace villas {
namespace dsp {

class PID {

protected:
	double dt;
	double max;
	double min;
	double Kp;
	double Kd;
	double Ki;
	double pre_error;
	double integral;

public:
	/**
	 *  Kp -  proportional gain
	 * Ki -  Integral gain
	 * Kd -  derivative gain
	 * dt -  loop interval time
	 * max - maximum value of manipulated variable
	 * min - minimum value of manipulated variable
	 */
	PID(double _dt, double _max, double _min, double _Kp, double _Kd, double _Ki);

	/** Returns the manipulated variable given a setpoint and current process value */
	double calculate(double setpoint, double pv);
};

} // namespace dsp
} // namespace villas
