/** An example get started with new implementations of new node-types
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/node/config.hpp>
#include <villas/format.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

struct example {
    /* Settings */
    int setting1;

    char *setting2;

    /* States */
    int state1;
    struct timespec start_time;
};

int example_type_start(SuperNode *sn);

int example_type_stop();

int example_init(NodeCompat *n);

int example_destroy(NodeCompat *n);

int example_parse(NodeCompat *n, json_t *json);

char * example_print(NodeCompat *n);

int example_check(NodeCompat *n);

int example_prepare(NodeCompat *n);

int example_start(NodeCompat *n);

int example_stop(NodeCompat *n);

int example_pause(NodeCompat *n);

int example_resume(NodeCompat *n);

int example_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int example_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int example_reverse(NodeCompat *n);

int example_poll_fds(NodeCompat *n, int fds[]);

int example_netem_fds(NodeCompat *n, int fds[]);

} /* namespace node */
} /* namespace villas */
