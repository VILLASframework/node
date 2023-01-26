/** Global include for tests.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#pragma once

#include <cstdlib>

#include <villas/fpga/pciecard.hpp>

class FpgaState {
public:
	// List of all available FPGA cards, only first will be tested at the moment
	std::list<std::shared_ptr<villas::fpga::PCIeCard>> cards;
};

// Global state to be shared by unittests
extern FpgaState state;
