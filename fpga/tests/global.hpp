#pragma once

#include <cstdlib>

#include <villas/fpga/card.hpp>

class FpgaState {
public:
	FpgaState() {
		// force criterion to only run one job at a time
		setenv("CRITERION_JOBS", "1", 0);
	}

	// list of all available FPGA cards, only first will be tested at the moment
	villas::fpga::CardList cards;
};

// global state to be shared by unittests
extern FpgaState state;


