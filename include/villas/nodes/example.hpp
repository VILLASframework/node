/** An example get started with new implementations of new node-types
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
 * @addtogroup example BSD example Node Type
 * @ingroup node
 * @{
 */

#pragma once

#include <villas/node/config.h>
#include <villas/node.h>
#include <villas/io.h>
#include <villas/timing.h>

struct example {
    /* Settings */
    int setting1;

    char *setting2;

    /* States */
    int state1;
    struct timespec start_time;
};

/** @see node_vtable::type_start */
int example_type_start(villas::node::SuperNode *sn);

/** @see node_type::type_stop */
int example_type_stop();

/** @see node_type::init */
int example_init(struct vnode *n);

/** @see node_type::destroy */
int example_destroy(struct vnode *n);

/** @see node_type::parse */
int example_parse(struct vnode *n, json_t *json);

/** @see node_type::print */
char * example_print(struct vnode *n);

/** @see node_type::check */
int example_check();

/** @see node_type::prepare */
int example_prepare();

/** @see node_type::start */
int example_start(struct vnode *n);

/** @see node_type::stop */
int example_stop(struct vnode *n);

/** @see node_type::pause */
int example_pause(struct vnode *n);

/** @see node_type::resume */
int example_resume(struct vnode *n);

/** @see node_type::write */
int example_write(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::read */
int example_read(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::reverse */
int example_reverse(struct vnode *n);

/** @see node_type::poll_fds */
int example_poll_fds(struct vnode *n, int fds[]);

/** @see node_type::netem_fds */
int example_netem_fds(struct vnode *n, int fds[]);

/** @} */
