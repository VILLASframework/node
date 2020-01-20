/** Unit tests for histogram
 *
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

#include <array>

#include <criterion/criterion.h>

#include <villas/hist.hpp>
#include <villas/utils.hpp>

const std::array<double, 10> test_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

using namespace villas;

TestSuite(hist, .description = "Histogram");

Test(hist, simple) {

	Hist h(10,2);

	for (auto td : test_data)
		h.put(td);

	cr_assert_float_eq(h.getMean(), 5.5, 1e-6, "Mean is %lf", h.getMean());
	cr_assert_float_eq(h.getVar(), 9.1666, 1e-3,);
	cr_assert_float_eq(h.getStddev(), 3.027650, 1e-6);
}
