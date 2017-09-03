/** Node-type for stats streaming.
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
 * @ingroup node
 * @addtogroup stats Sending stats
 * @{
 */

#pragma once

#include "task.h"

/* Forward declarations */
struct node;
struct path;
struct sample;

struct stats_node {
	struct task task;
	double rate;

	struct node *node;
};

/** @see node_type::print */
int stats_node_init(struct super_node *sn);

/** @see node_type::print */
char * stats_node_print(struct node *n);

/** @see node_type::parse */
int stats_node_parse(struct node *n, json_t *cfg);

/** @see node_type::start */
int stats_node_start(struct node *n);

/** @see node_type::stop */
int stats_node_stop(struct node *n);

/** @see node_type::read */
int stats_node_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */
