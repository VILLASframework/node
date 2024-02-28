/** A PID controller.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#include <iostream>
#include <cmath>
#include <villas/dsp/pid.hpp>

using namespace std;
using namespace villas::dsp;

PID::PID(double _dt, double _max, double _min, double _Kp, double _Kd, double _Ki) :
	dt(_dt),
	max(_max),
	min(_min),
	Kp(_Kp),
	Kd(_Kd),
	Ki(_Ki),
	pre_error(0),
	integral(0)
{ }

double PID::calculate(double setpoint, double pv)
{
	// Calculate error
	double error = setpoint - pv;

	// Proportional term
	double Pout = Kp * error;

	// Integral term
	integral += error * dt;
	double Iout = Ki * integral;

	// Derivative term
	double derivative = (error - pre_error) / dt;
	double Dout = Kd * derivative;

	// Calculate total output
	double output = Pout + Iout + Dout;

	// Restrict to max/min
	if (output > max)
		output = max;
	else if (output < min)
		output = min;

	// Save error to previous error
	pre_error = error;

	return output;
}
