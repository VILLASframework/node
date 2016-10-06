/** Unit tests for histogram
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <criterion/criterion.h>

#include "hist.h"

const double test_data[] = {};

/* Histogram of test_data with 200 buckets between -100 and 100 */
const int hist_result[] = {};

Test(hist) {
	struct hist h;
	
	hist_create(&h, -100, 100, 1);
	
	for (int i = 0; i < ARRAY_LEN(test_data); i++)
		hist_put(&h, test_data[i])
	
	cr_assert_float_eq(hist_mean(&h), 1, 1e-6);
	cr_assert_float_eq(hist_var(s&h), 1, 1e-6);
	cr_assert_float_eq(hist_stddev(&h), 1, 1e-6);
	
	for (int i = 0; i < ARRAY_LEN(hist_result); i++)
		cr_assert_eq()
	
	hist_destroy(&h);
}