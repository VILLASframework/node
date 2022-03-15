/** Node-type for stats streaming.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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

#include <jansson.h>

#include <villas/stats.hpp>
#include <villas/task.hpp>
#include <villas/list.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

struct stats_node_signal {
	Node *node;
	char *node_str;

	enum villas::Stats::Metric metric;
	enum villas::Stats::Type type;
};

struct stats_node {
	double rate;

	struct Task task;

	struct List signals; /** List of type struct stats_node_signal */
};

int stats_node_type_start(SuperNode *sn);

int stats_node_init(NodeCompat *n);

int stats_node_destroy(NodeCompat *n);

char *stats_node_print(NodeCompat *n);

int stats_node_parse(NodeCompat *n, json_t *json);

int stats_node_prepare(NodeCompat *n);

int stats_node_start(NodeCompat *n);

int stats_node_stop(NodeCompat *n);

int stats_node_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int stats_node_parse_signal(struct stats_node_signal *s, json_t *json);

int stats_node_signal_destroy(struct stats_node_signal *s);

int stats_node_signal_parse(struct stats_node_signal *s, json_t *json);

int stats_node_poll_fds(NodeCompat *n, int fds[]);

} /* namespace node */
} /* namespace villas */
