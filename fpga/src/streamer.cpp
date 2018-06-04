#include <csignal>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <jansson.h>

#include <CLI11.hpp>
#include <rang.hpp>

#include <villas/log.hpp>
#include <villas/utils.h>
#include <villas/utils.hpp>


#include <villas/fpga/ip.hpp>
#include <villas/fpga/card.hpp>

int main(int argc, const char* argv[])
{
	(void) argc;
	cxxopts::Options options(argv[0], " - example command line options");

	return 0;
}
