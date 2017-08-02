/** Node type: Node-type for testing Round-trip Time.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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

/**
 * @addtogroup test-rtt Node-type for testing Round-trip Time.
 * @ingroup node
 * @{
 */

#pragma once

#include "node.h"
#include "list.h"
#include "io.h"

struct test_rtt_case {
	int tfd;
	double rate;
	int values;
	int counter;
	int limit;		/**< The number of samples we take per test. */
};

struct test_rtt {
	double cooldown;	/**< Number of seconds to wait beween tests. */
	int current;		/**< Index of current test in test_rtt::cases */

	struct io io;		/**< The format of the output file */
	struct list cases;	/**< List of test cases */

	const char *output;	/**< The directory where we place the results. */
};

/** @see node_type::print */
char * test_rtt_print(struct node *n);

/** @see node_type::parse */
int test_rtt_parse(struct node *n, json_t *cfg);

/** @see node_type::open */
int test_rtt_start(struct node *n);

/** @see node_type::close */
int test_rtt_stop(struct node *n);

/** @see node_type::read */
int test_rtt_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::write */
int test_rtt_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */
