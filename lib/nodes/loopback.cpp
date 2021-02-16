/** Node-type for loopback connections.
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

#include <cstring>

#include <villas/node.h>
#include <villas/plugin.h>
#include <villas/node/config.h>
#include <villas/nodes/loopback.hpp>
#include <villas/exceptions.hpp>
#include <villas/memory.h>

using namespace villas;
using namespace villas::utils;

static struct plugin p;

int loopback_init(struct vnode *n)
{
	struct loopback *l = (struct loopback *) n->_vd;

	l->mode = QueueSignalledMode::AUTO;
	l->queuelen = DEFAULT_QUEUE_LENGTH;

	return 0;
}

int loopback_parse(struct vnode *n, json_t *json)
{
	struct loopback *l = (struct loopback *) n->_vd;
	const char *mode_str = nullptr;

	json_error_t err;
	int ret;

	ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: s }",
		"queuelen", &l->queuelen,
		"mode", &mode_str
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-loopback");

	if (mode_str) {
		if (!strcmp(mode_str, "auto"))
			l->mode = QueueSignalledMode::AUTO;
#ifdef HAVE_EVENTFD
		else if (!strcmp(mode_str, "eventfd"))
			l->mode = QueueSignalledMode::EVENTFD;
#endif
		else if (!strcmp(mode_str, "pthread"))
			l->mode = QueueSignalledMode::PTHREAD;
		else if (!strcmp(mode_str, "polling"))
			l->mode = QueueSignalledMode::POLLING;
#ifdef __APPLE__
		else if (!strcmp(mode_str, "pipe"))
			l->mode = QueueSignalledMode::PIPE;
#endif /* __APPLE__ */
		else
			throw ConfigError(json, "node-config-node-loopback-mode", "Unknown mode '{}'", mode_str);
	}

	return 0;
}

int loopback_prepare(struct vnode *n)
{
	struct loopback *l = (struct loopback *) n->_vd;

	return queue_signalled_init(&l->queue, l->queuelen, memory_default, l->mode);
}

int loopback_destroy(struct vnode *n)
{
	struct loopback *l= (struct loopback *) n->_vd;

	return queue_signalled_destroy(&l->queue);
}

int loopback_read(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int avail;

	struct loopback *l = (struct loopback *) n->_vd;
	struct sample *cpys[cnt];

	avail = queue_signalled_pull_many(&l->queue, (void **) cpys, cnt);

	for (int i = 0; i < avail; i++) {
		sample_copy(smps[i], cpys[i]);
		sample_decref(cpys[i]);
	}

	return avail;
}

int loopback_write(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct loopback *l = (struct loopback *) n->_vd;

	sample_incref_many(smps, cnt);

	return queue_signalled_push_many(&l->queue, (void **) smps, cnt);
}

char * loopback_print(struct vnode *n)
{
	struct loopback *l = (struct loopback *) n->_vd;
	char *buf = nullptr;

	strcatf(&buf, "queuelen=%d", l->queuelen);

	return buf;
}

int loopback_poll_fds(struct vnode *n, int fds[])
{
	struct loopback *l = (struct loopback *) n->_vd;

	fds[0] = queue_signalled_fd(&l->queue);

	return 1;
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "loopback";
	p.description		= "Loopback to connect multiple paths";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 0;
	p.node.flags		= 0;
	p.node.size		= sizeof(struct loopback);
	p.node.parse		= loopback_parse;
	p.node.print		= loopback_print;
	p.node.prepare		= loopback_prepare;
	p.node.init		= loopback_init;
	p.node.destroy		= loopback_destroy;
	p.node.read		= loopback_read;
	p.node.write		= loopback_write;
	p.node.poll_fds		= loopback_poll_fds;

	int ret = vlist_init(&p.node.instances);
	if (!ret)
		vlist_init_and_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
