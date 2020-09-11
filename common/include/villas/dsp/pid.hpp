/** A PID controller.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

} /* namespace dsp */
} /* namespace villas */
