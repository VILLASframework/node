/** Node type: Node-type for testing Round-trip Time.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <villas/list.hpp>
#include <villas/format.hpp>
#include <villas/task.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;
struct Sample;

struct test_rtt;

struct test_rtt_case {
	double rate;
	unsigned values;
	unsigned limit;			/**< The number of samples we take per test. */

	char *filename;
	char *filename_formatted;

	NodeCompat *node;
};

struct test_rtt {
	struct Task task;		/**< The periodic task for test_rtt_read() */
	Format *formatter;/**< The format of the output file */
	FILE *stream;

	double cooldown;		/**< Number of seconds to wait beween tests. */

	int current;			/**< Index of current test in test_rtt::cases */
	int counter;

	struct List cases;		/**< List of test cases */

	char *output;			/**< The directory where we place the results. */
	char *prefix;			/**< An optional prefix in the filename. */
};

char * test_rtt_print(NodeCompat *n);

int test_rtt_parse(NodeCompat *n, json_t *json);

int test_rtt_prepare(NodeCompat *n);

int test_rtt_init(NodeCompat *n);

int test_rtt_destroy(NodeCompat *n);

int test_rtt_start(NodeCompat *n);

int test_rtt_stop(NodeCompat *n);

int test_rtt_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int test_rtt_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int test_rtt_poll_fds(NodeCompat *n, int fds[]);

} /* namespace node */
} /* namespace villas */
