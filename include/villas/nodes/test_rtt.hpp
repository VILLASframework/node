/** Node type: Node-type for testing Round-trip Time.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/list.h>
#include <villas/format.hpp>
#include <villas/task.hpp>

/* Forward declarations */
struct test_rtt;
struct vnode;
struct sample;

struct test_rtt_case {
	double rate;
	unsigned values;
	unsigned limit;			/**< The number of samples we take per test. */

	char *filename;
	char *filename_formatted;

	struct vnode *node;
};

struct test_rtt {
	struct Task task;		/**< The periodic task for test_rtt_read() */
	villas::node::Format *formatter;/**< The format of the output file */
	FILE *stream;

	double cooldown;		/**< Number of seconds to wait beween tests. */

	int current;			/**< Index of current test in test_rtt::cases */
	int counter;

	struct vlist cases;		/**< List of test cases */

	char *output;			/**< The directory where we place the results. */
	char *prefix;			/**< An optional prefix in the filename. */
};

/** @see node_type::print */
char * test_rtt_print(struct vnode *n);

/** @see node_type::parse */
int test_rtt_parse(struct vnode *n, json_t *json);

/** @see node_type::start */
int test_rtt_start(struct vnode *n);

/** @see node_type::stop */
int test_rtt_stop(struct vnode *n);

/** @see node_type::read */
int test_rtt_read(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @see node_type::write */
int test_rtt_write(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @} */
