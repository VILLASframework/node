/** An example get started with new implementations of new node-types
 *
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

#include <villas/node_compat.hpp>
#include <villas/nodes/example.hpp>
#include <villas/utils.hpp>
#include <villas/sample.hpp>
#include <villas/exceptions.hpp>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

/* Forward declartions */
static NodeCompatType p;

int villas::node::example_type_start(villas::node::SuperNode *sn)
{
	/* TODO: Add implementation here */

	return 0;
}

int villas::node::example_type_stop()
{
	/* TODO: Add implementation here */

	return 0;
}

int villas::node::example_init(NodeCompat *n)
{
	auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. The following is just an example */

	s->setting1 = 0;
	s->setting2 = nullptr;

	return 0;
}

int villas::node::example_destroy(NodeCompat *n)
{
	// auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. The following is just an example */

	return 0;
}

int villas::node::example_parse(NodeCompat *n, json_t *json)
{
	int ret;
	auto *s = n->getData<struct example>();

	json_error_t err;

	/* TODO: Add implementation here. The following is just an example */

	ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: s }",
		"setting1", &s->setting1,
		"setting2", &s->setting2
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-example");

	return 0;
}

char * villas::node::example_print(NodeCompat *n)
{
	auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. The following is just an example */

	return strf("setting1=%d, setting2=%s", s->setting1, s->setting2);
}

int villas::node::example_check(NodeCompat *n)
{
	auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. The following is just an example */

	if (s->setting1 > 100 || s->setting1 < 0)
		return -1;

	if (!s->setting2 || strlen(s->setting2) > 10)
		return -1;

	return 0;
}

int villas::node::example_prepare(NodeCompat *n)
{
	auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. The following is just an example */

	s->state1 = s->setting1;

	if (strcmp(s->setting2, "double") == 0)
		s->state1 *= 2;

	return 0;
}

int villas::node::example_start(NodeCompat *n)
{
	auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. The following is just an example */

	s->start_time = time_now();

	return 0;
}

int villas::node::example_stop(NodeCompat *n)
{
	//auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. */

	return 0;
}

int villas::node::example_pause(NodeCompat *n)
{
	//auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. */

	return 0;
}

int villas::node::example_resume(NodeCompat *n)
{
	//auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. */

	return 0;
}

int villas::node::example_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int read;
	auto *s = n->getData<struct example>();
	struct timespec now;

	/* TODO: Add implementation here. The following is just an example */

	assert(cnt >= 1 && smps[0]->capacity >= 1);

	now = time_now();

	smps[0]->data[0].f = time_delta(&now, &s->start_time);

	/* Dont forget to set other flags in struct Sample::flags
	 * E.g. for sequence no, timestamps... */
	smps[0]->flags = (int) SampleFlags::HAS_DATA;
	smps[0]->signals = n->getInputSignals(false);

	read = 1; /* The number of samples read */

	return read;
}

int villas::node::example_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int written;
	//auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. */

	written = 0; /* The number of samples written */

	return written;
}

int villas::node::example_reverse(NodeCompat *n)
{
	//auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. */

	return 0;
}

int villas::node::example_poll_fds(NodeCompat *n, int fds[])
{
	//auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. */

	return 0; /* The number of file descriptors which have been set in fds */
}

int villas::node::example_netem_fds(NodeCompat *n, int fds[])
{
	//auto *s = n->getData<struct example>();

	/* TODO: Add implementation here. */

	return 0; /* The number of file descriptors which have been set in fds */
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "example";
	p.description		= "An example for staring new node-type implementations";
	p.vectorize	= 0;
	p.size		= sizeof(struct example);
	p.type.start	= example_type_start;
	p.type.stop	= example_type_stop;
	p.init		= example_init;
	p.destroy	= example_destroy;
	p.prepare	= example_prepare;
	p.parse		= example_parse;
	p.print		= example_print;
	p.check		= example_check;
	p.start		= example_start;
	p.stop		= example_stop;
	p.pause		= example_pause;
	p.resume	= example_resume;
	p.read		= example_read;
	p.write		= example_write;
	p.reverse	= example_reverse;
	p.poll_fds	= example_poll_fds;
	p.netem_fds	= example_netem_fds;

	static NodeCompatFactory ncp(&p);
}
