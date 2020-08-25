/** Node-type for stats streaming.
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
 * @ingroup node
 * @addtogroup stats Sending stats
 * @{
 */

#pragma once

#include <jansson.h>

#include <villas/node.h>
#include <villas/stats.hpp>
#include <villas/task.hpp>
#include <villas/list.h>

struct stats_node_signal {
	struct vnode *node;
	char *node_str;

	enum villas::Stats::Metric metric;
	enum villas::Stats::Type type;
};

struct stats_node {
	double rate;

	struct Task task;

	struct vlist signals; /** List of type struct stats_node_signal */
};

/** @see node_type::print */
int stats_node_type_start(villas::node::SuperNode *sn);

/** @see node_type::print */
char *stats_node_print(struct vnode *n);

/** @see node_type::parse */
int stats_node_parse(struct vnode *n, json_t *cfg);

int stats_node_parse_signal(struct stats_node_signal *s, json_t *cfg);

/** @see node_type::start */
int stats_node_start(struct vnode *n);

/** @see node_type::stop */
int stats_node_stop(struct vnode *n);

/** @see node_type::read */
int stats_node_read(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @} */
