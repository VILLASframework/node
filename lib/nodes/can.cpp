/** Node-type: CAN bus
 *
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

#include <villas/nodes/can.hpp>
#include <villas/utils.hpp>
#include <villas/sample.h>
#include <villas/plugin.h>
#include <villas/super_node.hpp>

/* Forward declartions */
static struct plugin p;

using namespace villas::node;
using namespace villas::utils;

int can_type_start(villas::node::SuperNode *sn)
{
	/* TODO: Add implementation here */

	return 0;
}

int can_type_stop()
{
	/* TODO: Add implementation here */

	return 0;
}

int can_init(struct node *n)
{
	struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. The following is just an can */

	c->setting1 = 0;
	c->setting2 = nullptr;

	return 0;
}

int can_destroy(struct node *n)
{
	struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. The following is just an can */

	if (c->setting2)
		free(c->setting2);

	return 0;
}

int can_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct can *c = (struct can *) n->_vd;

	json_error_t err;

	/* TODO: Add implementation here. The following is just an can */

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: s }",
		"setting1", &c->setting1,
		"setting2", &c->setting2
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	return 0;
}

char * can_print(struct node *n)
{
	struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. The following is just an can */

	return strf("setting1=%d, setting2=%s", c->setting1, c->setting2);
}

int can_check(struct node *n)
{
	struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. The following is just an can */

	if (c->setting1 > 100 || c->setting1 < 0)
		return -1;

	if (!c->setting2 || strlen(c->setting2) > 10)
		return -1;

	return 0;
}

int can_prepare(struct node *n)
{
	struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. The following is just an can */

	c->state1 = c->setting1;

	if (strcmp(c->setting2, "double") == 0)
		c->state1 *= 2;

	return 0;
}

int can_start(struct node *n)
{
	struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. The following is just an can */

	c->start_time = time_now();

	return 0;
}

int can_stop(struct node *n)
{
	//struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int can_pause(struct node *n)
{
	//struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int can_resume(struct node *n)
{
	//struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int can_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int read;
	struct can *c = (struct can *) n->_vd;
	struct timespec now;

	/* TODO: Add implementation here. The following is just an can */

	assert(cnt >= 1 && smps[0]->capacity >= 1);

	now = time_now();

	smps[0]->data[0].f = time_delta(&now, &c->start_time);

	read = 1; /* The number of samples read */

	return read;
}

int can_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int written;
	//struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. */

	written = 0; /* The number of samples written */

	return written;
}

int can_reverse(struct node *n)
{
	//struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int can_poll_fds(struct node *n, int fds[])
{
	//struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. */

	return 0; /* The number of file descriptors which have been set in fds */
}

int can_netem_fds(struct node *n, int fds[])
{
	//struct can *c = (struct can *) n->_vd;

	/* TODO: Add implementation here. */

	return 0; /* The number of file descriptors which have been set in fds */
}

__attribute__((constructor(110)))
static void register_plugin() {
	if (plugins.state == State::DESTROYED)
		vlist_init(&plugins);

	p.name			= "can";
	p.description		= "CAN bus for Xilinx MPSoC boards";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct can);
	p.node.type.start	= can_type_start;
	p.node.type.stop	= can_type_stop;
	p.node.init		= can_init;
	p.node.destroy		= can_destroy;
	p.node.prepare		= can_prepare;
	p.node.parse		= can_parse;
	p.node.print		= can_print;
	p.node.check		= can_check;
	p.node.start		= can_start;
	p.node.stop		= can_stop;
	p.node.pause		= can_pause;
	p.node.resume		= can_resume;
	p.node.read		= can_read;
	p.node.write		= can_write;
	p.node.reverse		= can_reverse;
	p.node.poll_fds		= can_poll_fds;
	p.node.netem_fds	= can_netem_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	if (plugins.state != State::DESTROYED)
		vlist_remove_all(&plugins, &p);
}
