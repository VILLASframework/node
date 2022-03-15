/** A PID controller.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	/* Calculate error */
	double error = setpoint - pv;

	/* Proportional term */
	double Pout = Kp * error;

	/* Integral term */
	integral += error * dt;
	double Iout = Ki * integral;

	/* Derivative term */
	double derivative = (error - pre_error) / dt;
	double Dout = Kd * derivative;

	/* Calculate total output */
	double output = Pout + Iout + Dout;

	/* Restrict to max/min */
	if (output > max)
		output = max;
	else if (output < min)
		output = min;

	/* Save error to previous error */
	pre_error = error;

	return output;
}
