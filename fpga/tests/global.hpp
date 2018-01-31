#pragma once

#include <cstdlib>

#include <villas/fpga/card.hpp>

class FpgaState {
public:
	// list of all available FPGA cards, only first will be tested at the moment
	villas::fpga::CardList cards;
};

// global state to be shared by unittests
extern FpgaState state;
