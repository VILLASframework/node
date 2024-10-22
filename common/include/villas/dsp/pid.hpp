/* A PID controller.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

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
  //  Kp -  proportional gain
  // Ki -  Integral gain
  // Kd -  derivative gain
  // dt -  loop interval time
  // max - maximum value of manipulated variable
  // min - minimum value of manipulated variable
  PID(double _dt, double _max, double _min, double _Kp, double _Kd, double _Ki);

  // Returns the manipulated variable given a setpoint and current process value
  double calculate(double setpoint, double pv);
};

} // namespace dsp
} // namespace villas
