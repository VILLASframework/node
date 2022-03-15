/** Node-type for shared memory communication.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
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

#include <fcntl.h>
#include <pthread.h>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <villas/kernel/kernel.hpp>
#include <villas/log.hpp>
#include <villas/exceptions.hpp>
#include <villas/shmem.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/shmem.hpp>
#include <villas/timing.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

int villas::node::shmem_init(NodeCompat *n)
{
	auto *shm = n->getData<struct shmem>();

	/* Default values */
	shm->conf.queuelen = -1;
	shm->conf.samplelen = -1;
	shm->conf.polling = false;
	shm->exec = nullptr;

	return 0;
}

int villas::node::shmem_parse(NodeCompat *n, json_t *json)
{
	auto *shm = n->getData<struct shmem>();
	const char *val, *mode_str = nullptr;

	int ret;
	json_t *json_exec = nullptr;
	json_error_t err;

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

int villas::node::shmem_prepare(NodeCompat *n)
{
	auto *shm = n->getData<struct shmem>();

	if (shm->conf.queuelen < 0)
		shm->conf.queuelen = MAX(DEFAULT_SHMEM_QUEUELEN, n->in.vectorize);

	if (shm->conf.samplelen < 0)  {
		auto input_sigs = n->getInputSignals(false)->size();
		auto output_sigs = 0U;

		if (n->getOutputSignals(true))
			output_sigs = n->getOutputSignals(true)->size();

		shm->conf.samplelen = MAX(input_sigs, output_sigs);
	}

	return 0;
}

int villas::node::shmem_start(NodeCompat *n)
{
	auto *shm = n->getData<struct shmem>();
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

int villas::node::shmem_stop(NodeCompat *n)
{
	auto* shm = n->getData<struct shmem>();

	return shmem_int_close(&shm->intf);
}

int villas::node::shmem_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	auto *shm = n->getData<struct shmem>();
	int recv;
	struct Sample *shared_smps[cnt];

	do {
		recv = shmem_int_read(&shm->intf, shared_smps, cnt);
	} while (recv == 0);

	if (recv < 0) {
		/* This can only really mean that the other process has exited, so close
		 * the interface to make sure the shared memory object is unlinked */

		n->logger->info("Shared memory segment has been closed.");

		n->setState(State::STOPPING);

		return recv;
	}

	sample_copy_many(smps, shared_smps, recv);
	sample_decref_many(shared_smps, recv);

	/** @todo signal descriptions are currently not shared between processes */
	for (int i = 0; i < recv; i++)
		smps[i]->signals = n->getInputSignals(false);

	return recv;
}

int villas::node::shmem_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	auto *shm = n->getData<struct shmem>();
	struct Sample *shared_smps[cnt]; /* Samples need to be copied to the shared pool first */
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

char * villas::node::shmem_print(NodeCompat *n)
{
	auto *shm = n->getData<struct shmem>();
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

static NodeCompatType p;

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
	p.prepare	= shmem_prepare;
	p.init		= shmem_init;

	static NodeCompatFactory ncp(&p);
}
