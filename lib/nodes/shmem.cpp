/** Node-type for shared memory communication.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
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

#include <fcntl.h>
#include <pthread.h>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <villas/kernel/kernel.hpp>
#include <villas/log.hpp>
#include <villas/exceptions.hpp>
#include <villas/shmem.h>
#include <villas/node.h>
#include <villas/nodes/shmem.hpp>
#include <villas/timing.h>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

int shmem_parse(struct vnode *n, json_t *json)
{
	struct shmem *shm = (struct shmem *) n->_vd;
	const char *val, *mode_str = nullptr;

	int ret;
	json_t *json_exec = nullptr;
	json_error_t err;

	int len = MAX(vlist_length(&n->in.signals), vlist_length(&n->out.signals));

	/* Default values */
	shm->conf.queuelen = MAX(DEFAULT_SHMEM_QUEUELEN, n->in.vectorize);
	shm->conf.samplelen = len;
	shm->conf.polling = false;
	shm->exec = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s: { s: s }, s: { s: s }, s?: i, s?: o, s?: s }",
		"out",
			"name", &shm->out_name,
		"in",
			"name", &shm->in_name,
		"queuelen", &shm->conf.queuelen,
		"exec", &json_exec,
		"mode", &mode_str
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-shmem");

	if (mode_str) {
		if (!strcmp(mode_str, "polling"))
			shm->conf.polling = true;
		else if (!strcmp(mode_str, "pthread"))
			shm->conf.polling = false;
		else
			throw SystemError("Unknown mode '{}'", mode_str);
	}

	if (json_exec) {
		if (!json_is_array(json_exec))
			throw SystemError("Setting 'exec' must be an array of strings");

		shm->exec = new char*[json_array_size(json_exec) + 1];
		if (!shm->exec)
			throw MemoryAllocationError();

		size_t i;
		json_t *json_val;
		json_array_foreach(json_exec, i, json_val) {
			val = json_string_value(json_val);
			if (!val)
				throw SystemError("Setting 'exec' must be an array of strings");

			shm->exec[i] = strdup(val);
		}

		shm->exec[i] = nullptr;
	}

	return 0;
}

int shmem_start(struct vnode *n)
{
	struct shmem *shm = (struct shmem *) n->_vd;
	int ret;

	if (shm->exec) {
		ret = spawn(shm->exec[0], shm->exec);
		if (!ret)
			throw SystemError("Failed to spawn external program");

		sleep(1);
	}

	ret = shmem_int_open(shm->out_name, shm->in_name, &shm->intf, &shm->conf);
	if (ret < 0)
		throw SystemError("Opening shared memory interface failed (ret={})", ret);

	return 0;
}

int shmem_stop(struct vnode *n)
{
	struct shmem* shm = (struct shmem *) n->_vd;

	return shmem_int_close(&shm->intf);
}

int shmem_read(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	struct shmem *shm = (struct shmem *) n->_vd;
	int recv;
	struct sample *shared_smps[cnt];

	do {
		recv = shmem_int_read(&shm->intf, shared_smps, cnt);
	} while (recv == 0);

	if (recv < 0) {
		/* This can only really mean that the other process has exited, so close
		 * the interface to make sure the shared memory object is unlinked */

		n->logger->info("Shared memory segment has been closed.");

		n->state = State::STOPPING;

		return recv;
	}

	sample_copy_many(smps, shared_smps, recv);
	sample_decref_many(shared_smps, recv);

	/** @todo: signal descriptions are currently not shared between processes */
	for (int i = 0; i < recv; i++)
		smps[i]->signals = &n->in.signals;

	return recv;
}

int shmem_write(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	struct shmem *shm = (struct shmem *) n->_vd;
	struct sample *shared_smps[cnt]; /* Samples need to be copied to the shared pool first */
	int avail, pushed, copied;

	avail = sample_alloc_many(&shm->intf.write.shared->pool, shared_smps, cnt);
	if (avail != (int) cnt)
		n->logger->warn("Pool underrun for shmem node {}", shm->out_name);

	copied = sample_copy_many(shared_smps, smps, avail);
	if (copied < avail)
		n->logger->warn("Outgoing pool underrun");

	pushed = shmem_int_write(&shm->intf, shared_smps, copied);
	if (pushed != avail)
		n->logger->warn("Outgoing queue overrun for node");

	return pushed;
}

char * shmem_print(struct vnode *n)
{
	struct shmem *shm = (struct shmem *) n->_vd;
	char *buf = nullptr;

	strcatf(&buf, "out_name=%s, in_name=%s, queuelen=%d, polling=%s",
		shm->out_name, shm->in_name, shm->conf.queuelen, shm->conf.polling ? "yes" : "no");

	if (shm->exec) {
		strcatf(&buf, ", exec='");

		for (int i = 0; shm->exec[i]; i++)
			strcatf(&buf, shm->exec[i+1] ? "%s " : "%s", shm->exec[i]);

		strcatf(&buf, "'");
	}

	return buf;
}

static struct vnode_type p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "shmem";
	p.description	= "POSIX shared memory interface with external processes";
	p.vectorize	= 0;
	p.size		= sizeof(struct shmem);
	p.parse		= shmem_parse;
	p.print		= shmem_print;
	p.start		= shmem_start;
	p.stop		= shmem_stop;
	p.read		= shmem_read;
	p.write		= shmem_write;

	if (!node_types)
		node_types = new NodeTypeList();

	node_types->push_back(&p);
}
