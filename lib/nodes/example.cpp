/** An example get started with new implementations of new node-types
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

#include <villas/nodes/example.hpp>
#include <villas/utils.hpp>
#include <villas/sample.h>
#include <villas/plugin.h>
#include <villas/super_node.hpp>

/* Forward declartions */
static struct plugin p;

using namespace villas::node;
using namespace villas::utils;

int example_type_start(villas::node::SuperNode *sn)
{
	/* TODO: Add implementation here */

	return 0;
}

int example_type_stop()
{
	/* TODO: Add implementation here */

	return 0;
}

int example_init(struct node *n)
{
	struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. The following is just an example */

	s->setting1 = 0;
	s->setting2 = nullptr;

	return 0;
}

int example_destroy(struct node *n)
{
	struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. The following is just an example */

	if (s->setting2)
		free(s->setting2);

	return 0;
}

int example_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct example *s = (struct example *) n->_vd;

	json_error_t err;

	/* TODO: Add implementation here. The following is just an example */

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: s }",
		"setting1", &s->setting1,
		"setting2", &s->setting2
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	return 0;
}

char * example_print(struct node *n)
{
	struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. The following is just an example */

	return strf("setting1=%d, setting2=%s", s->setting1, s->setting2);
}

int example_check(struct node *n)
{
	struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. The following is just an example */

	if (s->setting1 > 100 || s->setting1 < 0)
		return -1;

	if (!s->setting2 || strlen(s->setting2) > 10)
		return -1;

	return 0;
}

int example_prepare(struct node *n)
{
	struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. The following is just an example */

	s->state1 = s->setting1;

	if (strcmp(s->setting2, "double") == 0)
		s->state1 *= 2;

	return 0;
}

int example_start(struct node *n)
{
	struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. The following is just an example */

	s->start_time = time_now();

	return 0;
}

int example_stop(struct node *n)
{
	//struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int example_pause(struct node *n)
{
	//struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int example_resume(struct node *n)
{
	//struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int example_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int read;
	struct example *s = (struct example *) n->_vd;
	struct timespec now;

	/* TODO: Add implementation here. The following is just an example */

	assert(cnt >= 1 && smps[0]->capacity >= 1);

	now = time_now();

	smps[0]->data[0].f = time_delta(&now, &s->start_time);

	read = 1; /* The number of samples read */

	return read;
}

int example_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int written;
	//struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. */

	written = 0; /* The number of samples written */

	return written;
}

int example_reverse(struct node *n)
{
	//struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. */

	return 0;
}

int example_poll_fds(struct node *n, int fds[])
{
	//struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. */

	return 0; /* The number of file descriptors which have been set in fds */
}

int example_netem_fds(struct node *n, int fds[])
{
	//struct example *s = (struct example *) n->_vd;

	/* TODO: Add implementation here. */

	return 0; /* The number of file descriptors which have been set in fds */
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "example";
	p.description		= "An example for staring new node-type implementations";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct example);
	p.node.type.start	= example_type_start;
	p.node.type.stop	= example_type_stop;
	p.node.init		= example_init;
	p.node.destroy		= example_destroy;
	p.node.prepare		= example_prepare;
	p.node.parse		= example_parse;
	p.node.print		= example_print;
	p.node.check		= example_check;
	p.node.start		= example_start;
	p.node.stop		= example_stop;
	p.node.pause		= example_pause;
	p.node.resume		= example_resume;
	p.node.read		= example_read;
	p.node.write		= example_write;
	p.node.reverse		= example_reverse;
	p.node.poll_fds		= example_poll_fds;
	p.node.netem_fds	= example_netem_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
