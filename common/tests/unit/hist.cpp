/** Unit tests for histogram
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#include <array>

#include <criterion/criterion.h>

#include <villas/hist.hpp>
#include <villas/utils.hpp>

const std::array<double, 10> test_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

using namespace villas;

// cppcheck-suppress unknownMacro
TestSuite(hist, .description = "Histogram");

Test(hist, simple) {

	Hist h(10,2);

	for (auto td : test_data)
		h.put(td);

	cr_assert_float_eq(h.getMean(), 5.5, 1e-6, "Mean is %lf", h.getMean());
	cr_assert_float_eq(h.getVar(), 9.1666, 1e-3);
	cr_assert_float_eq(h.getStddev(), 3.027650, 1e-6);
}
