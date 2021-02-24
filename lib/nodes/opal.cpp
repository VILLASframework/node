/** Node type: OPAL (AsyncApi)
 *
 * This file implements the opal subtype for nodes.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <cmath>
#include <vector>

#include <villas/nodes/opal.hpp>
#include <villas/utils.hpp>
#include <villas/plugin.h>
#include <villas/exceptions.hpp>

extern "C" {
	/* Define RTLAB before including OpalPrint.h for messages to be sent
	* to the OpalDisplay. Otherwise stdout will be used. */
	#define RTLAB
	#include <OpalPrint.h>
	#include <AsyncApi.h>
	#include <OpalGenAsyncParamCtrl.h>
}


/* Private static storage */
static std::string asyncShmemName;			/**< Shared Memory identifiers and size, provided via argv. */
static std::string printShmemName;			/**< Shared Memory identifiers and size, provided via argv. */
static size_t asyncShmemSize;				/**< Shared Memory identifiers and size, provided via argv. */

static std::vector<unsigned> sendIDs, recvIDs;	/** A dynamically allocated array of SendIDs. */

static Opal_GenAsyncParam_Ctrl params;			/** String and Float parameters, provided by the OPAL AsyncProcess block. */

static pthread_mutex_t lock;				/** Big Global Lock for libOpalAsync API */

using namespace villas;
using namespace villas::utils;

extern "C" {
	int __xstat(int ver, const char * path, struct stat * stat_buf)
	{
		return stat(path, stat_buf);
	}

	int backtrace(void **buffer, int size)
	{
		return 0;
	}

	char **backtrace_symbols(void *const *buffer, int size)
	{
		return nullptr;
	}

	void backtrace_symbols_fd(void *const *buffer, int size, int fd)
	{
	}

	void * _intel_fast_memset(void *b, int c, size_t len)
	{
		return memset(b, c, len);
	}

	void * _intel_fast_memcpy(void *dst, const void *src, size_t n)
	{
		return memcpy(dst, src, n);
	}

	int _intel_fast_memcmp(const void *s1, const void *s2, size_t n)
	{
		return memcmp(s1, s2, n);
	}
}

int opal_register_region(int argc, char *argv[])
{
	asyncShmemName = argv[1];
	asyncShmemSize = atoi(argv[2]);
	printShmemName = argv[3];

	return 0;
}

int opal_type_start(villas::node::SuperNode *sn)
{
	int err, noRecvIcons, noSendIcons;

	/* @todo: Port to C++
	if (sn->cli.argc != 4)
		return -1; */

	pthread_mutex_init(&lock, nullptr);

	/* Enable the OpalPrint function. This prints to the OpalDisplay. */
	err = OpalSystemCtrl_Register((char *) printShmemName.c_str());
	if (err != EOK)
		error("OpalPrint() access not available (%d)", err);

	/* Open Share Memory created by the model. */
	err = OpalOpenAsyncMem(asyncShmemSize, asyncShmemName.c_str());
	if (err != EOK)
		error("Model shared memory not available (%d)", err);

	err = OpalGetAsyncCtrlParameters(&params, sizeof(Opal_GenAsyncParam_Ctrl));
	if (err != EOK)
		error("Could not get OPAL controller parameters (%d)", err);

	/* Get list of Send and RecvIDs */
	err = OpalGetNbAsyncSendIcon(&noSendIcons);
	if (err != EOK)
		error("Failed to get number of send blocks (%d)", err);
	err = OpalGetNbAsyncRecvIcon(&noRecvIcons);
	if (err != EOK)
		error("Failed to get number of recv blocks (%d)", err);

	sendIDs.resize(noSendIcons);
	recvIDs.resize(noRecvIcons);

	err = OpalGetAsyncSendIDList(sendIDs.data(), noSendIcons * sizeof(int));
	if (err != EOK)
		error("Failed to get list of send ids (%d)", err);

	err = OpalGetAsyncRecvIDList(recvIDs.data(), noRecvIcons * sizeof(int));
	if (err != EOK)
		error("Failed to get list of recv ids (%d)", err);

	info("Started as OPAL Asynchronous process");
	info("This is VILLASnode %s (built on %s, %s)",
		PROJECT_BUILD_ID, __DATE__, __TIME__);

	opal_print_global();

	return 0;
}

int opal_type_stop()
{
	int err;

	err = OpalCloseAsyncMem(asyncShmemSize, asyncShmemName.c_str());
	if (err != EOK)
		error("Failed to close shared memory area (%d)", err);

	debug(LOG_OPAL | 4, "Closing OPAL shared memory mapping");

	err = OpalSystemCtrl_UnRegister((char *) printShmemName.c_str());
	if (err != EOK)
		error("Failed to close shared memory for system control (%d)", err);

	pthread_mutex_destroy(&lock);

	return 0;
}

int opal_print_global()
{
	debug(LOG_OPAL | 2, "Controller ID: %u", params.controllerID);

	std::stringstream sss, rss;

	for (auto i : sendIDs)
		sss << i << " ";
	for (auto i : recvIDs)
		rss << i << " ";

	debug(LOG_OPAL | 2, "Send Blocks: %s",    sss.str().c_str());
	debug(LOG_OPAL | 2, "Receive Blocks: %s", rss.str().c_str());

	debug(LOG_OPAL | 2, "Control Block Parameters:");

	for (int i = 0; i < GENASYNC_NB_FLOAT_PARAM; i++)
		debug(LOG_OPAL | 2, "FloatParam[]%u] = %f", i, params.FloatParam[i]);

	for (int i = 0; i < GENASYNC_NB_STRING_PARAM; i++)
		debug(LOG_OPAL | 2, "StringParam[%u] = %s", i, params.StringParam[i]);

	return 0;
}

int opal_parse(struct vnode *n, json_t *cfg)
{
	struct opal *o = (struct opal *) n->_vd;

	int ret;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: i, s: i, s: b }",
		"sendID", &o->sendID,
		"recvID", &o->recvID,
		"reply", &o->reply
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	return 0;
}

char * opal_print(struct vnode *n)
{
	struct opal *o = (struct opal *) n->_vd;

	/** @todo: Print send_params, recv_params */

	return strf("sendID=%u, recvID=%u, reply=%u",
		o->sendID, o->recvID, o->reply);
}

int opal_start(struct vnode *n)
{
	struct opal *o = (struct opal *) n->_vd;

	/* Search for valid send and recv ids */
	int sfound = 0, rfound = 0;
	for (auto i : sendIDs)
		sfound += i == o->sendID;
	for (auto i : recvIDs)
		rfound += i == o->sendID;

	if (!sfound)
		error("Invalid sendID '%u' for node %s", o->sendID, node_name(n));
	if (!rfound)
		error("Invalid recvID '%u' for node %s", o->recvID, node_name(n));

	/* Get some more informations and paramters from OPAL-RT */
	OpalGetAsyncSendIconMode(&o->mode, o->sendID);
	OpalGetAsyncSendParameters(&o->sendParams, sizeof(Opal_SendAsyncParam), o->sendID);
	OpalGetAsyncRecvParameters(&o->recvParams, sizeof(Opal_RecvAsyncParam), o->recvID);

	o->sequenceNo = 0;

	return 0;
}

int opal_read(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct opal *o = (struct opal *) n->_vd;

	int state, ret, len;
	unsigned id;

	struct sample *s = smps[0];

	double data[s->capacity];

	if (cnt != 1)
		error("The OPAL-RT node type does not support combining!");

	/* This call unblocks when the 'Data Ready' line of a send icon is asserted. */
	do {
		ret = OpalWaitForAsyncSendRequest(&id);
		if (ret != EOK) {
			state = OpalGetAsyncModelState();
			if ((state == STATE_RESET) || (state == STATE_STOP))
				error("OpalGetAsyncModelState(): Model stopped or resetted!");

			return -1; /* @todo correct return value */
		}
	} while (id != o->sendID);

	/* No errors encountered yet */
	OpalSetAsyncSendIconError(0, o->sendID);

	/* Get the size of the data being sent by the unblocking SendID */
	OpalGetAsyncSendIconDataLength(&len, o->sendID);
	if ((unsigned) len > s->capacity * sizeof(s->data[0])) {
		warning("Ignoring the last %u of %u values for OPAL node %s (sendID=%u).",
		len / sizeof(double) - s->capacity, len / sizeof(double), node_name(n), o->sendID);

		len = sizeof(data);
	}

	/* Read data from the model */
	OpalGetAsyncSendIconData(data, len, o->sendID);

	s->sequence = htons(o->sequenceNo++);
	s->length = (unsigned) len / sizeof(double);

	for (unsigned i = 0; i < s->length; i++)
		s->data[i].f = (float) data[i]; /* OPAL provides double precission */

	/* This next call allows the execution of the "asynchronous" process
	 * to actually be synchronous with the model. To achieve this, you
	 * should set the "Sending Mode" in the Async_Send block to
	 * NEED_REPLY_BEFORE_NEXT_SEND or NEED_REPLY_NOW. This will force
	 * the model to wait for this process to call this
	 * OpalAsyncSendRequestDone function before continuing. */
	if (o->reply)
		OpalAsyncSendRequestDone(o->sendID);

	/* Before continuing, we make sure that the real-time model
	 * has not been stopped. If it has, we quit. */
	state = OpalGetAsyncModelState();
	if ((state == STATE_RESET) || (state == STATE_STOP))
		error("OpalGetAsyncModelState(): Model stopped or resetted!");

	return 1;
}

int opal_write(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct opal *o = (struct opal *) n->_vd;

	struct sample *s = smps[0];

	int state;
	int len;
	double data[s->length];

	if (cnt != 1)
		error("The OPAL-RT node type does not support combining!");

	state = OpalGetAsyncModelState();
	if ((state == STATE_RESET) || (state == STATE_STOP))
		error("OpalGetAsyncModelState(): Model stopped or resetted!");

	OpalSetAsyncRecvIconStatus(s->sequence, o->recvID);	/* Set the Status to the message ID */
	OpalSetAsyncRecvIconError(0, o->recvID);		/* Set the Error to 0 */

	/* Get the number of signals to send back to the model */
	OpalGetAsyncRecvIconDataLength(&len, o->recvID);
	if ((unsigned) len > sizeof(data))
		warning("Node %s is expecting more signals (%u) than values in message (%u)", node_name(n), len / sizeof(double), s->length);

	for (unsigned i = 0; i < s->length; i++)
		data[i] = (double) s->data[i].f; /* OPAL expects double precission */

	OpalSetAsyncRecvIconData(data, s->length * sizeof(double), o->recvID);

	return 1;
}

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "opal";
	p.description		= "run as OPAL Asynchronous Process (libOpalAsyncApi)";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 1;
	p.node.size		= sizeof(struct opal);
	p.node.type.start	= opal_type_start;
	p.node.type.stop	= opal_type_stop;
	p.node.parse		= opal_parse;
	p.node.print		= opal_print;
	p.node.start		= opal_start;
	p.node.read		= opal_read;
	p.node.write		= opal_write;

	int ret = vlist_init(&p.node.instances);
	if (!ret)
		vlist_init_and_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
