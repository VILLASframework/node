/** Node type: OPAL (AsyncApi)
 *
 * This file implements the opal subtype for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <math.h>

#include "opal.h"
#include "utils.h"

static struct opal_global *og = NULL;

int opal_init(int argc, char *argv[])
{	
	int err;
		
	if (argc != 4)
		return -1;

	struct opal_global *g = alloc(sizeof(struct opal_global));
	
	pthread_mutex_init(&g->lock, NULL);
	
	g->async_shmem_name = argv[1];
	g->async_shmem_size = atoi(argv[2]);
	g->print_shmem_name = argv[3];
	
	/* Enable the OpalPrint function. This prints to the OpalDisplay. */
	if ((err = OpalSystemCtrl_Register(g->print_shmem_name)) != EOK)
		error("OpalPrint() access not available (%d)", err);
	
	/* Open Share Memory created by the model. */
	if ((err = OpalOpenAsyncMem(g->async_shmem_size, g->async_shmem_name)) != EOK)
		error("Model shared memory not available (%d)", err);

	if ((err = OpalGetAsyncCtrlParameters(&g->params, sizeof(Opal_GenAsyncParam_Ctrl))) != EOK)
		error("Could not get OPAL controller parameters (%d)", err);
	
	/* Get list of Send and RecvIDs */
	if ((err = OpalGetNbAsyncSendIcon(&g->send_icons)) != EOK)
		error("Failed to get number of send blocks (%d)", err);
	if ((err = OpalGetNbAsyncRecvIcon(&g->recv_icons)) != EOK)
		error("Failed to get number of recv blocks (%d)", err);
	
	g->send_ids = alloc(g->send_icons * sizeof(int));
	g->recv_ids = alloc(g->recv_icons * sizeof(int));
	
	if ((err = OpalGetAsyncSendIDList(g->send_ids, g->send_icons * sizeof(int))) != EOK)
		error("Failed to get list of send ids (%d)", err);
	if ((err = OpalGetAsyncRecvIDList(g->recv_ids, g->recv_icons * sizeof(int))) != EOK)
		error("Failed to get list of recv ids (%d)", err);
	
	info("Started as OPAL Asynchronous process");
	info("This is Simulator2Simulator Server (S2SS) %s (built on %s, %s, debug=%d)",
		VERSION, __DATE__, __TIME__, _debug);
	opal_print_global(g);
	
	og = g;
	
	return 0;
}

int opal_deinit()
{
	int err;

	if (!og)
		return 0;
	
	if ((err = OpalCloseAsyncMem(og->async_shmem_size, og->async_shmem_name)) != EOK)
		error("Failed to close shared memory area (%d)", err);
	
	debug(4, "Closing OPAL shared memory mapping");
	
	if ((err = OpalSystemCtrl_UnRegister(og->print_shmem_name)) != EOK)
		error("Failed to close shared memory for system control (%d)", err);
		
	free(og->send_ids);
	free(og->recv_ids);
	free(og);

	og = NULL;
	
	return 0;
}

int opal_print_global(struct opal_global *g)
{ INDENT
	char sbuf[512] = "";
	char rbuf[512] = "";
	
	for (int i=0; i<g->send_icons; i++)
		strap(sbuf, sizeof(sbuf), "%u ", g->send_ids[i]);
	for (int i=0; i<g->recv_icons; i++)
		strap(rbuf, sizeof(rbuf), "%u ", g->recv_ids[i]);
	
	debug(2, "Controller ID: %u", g->params.controllerID);
	debug(2, "Send Blocks: %s",    sbuf);
	debug(2, "Receive Blocks: %s", rbuf);

	debug(2, "Control Block Parameters:");
	for (int i=0; i<GENASYNC_NB_FLOAT_PARAM; i++)
		debug(2, "FloatParam[]%u] = %f", i, g->params.FloatParam[i]);
	for (int i=0; i<GENASYNC_NB_STRING_PARAM; i++)
		debug(2, "StringParam[%u] = %s", i, g->params.StringParam[i]);
	
	return 0;
}

int opal_parse(config_setting_t *cfg, struct node *n)
{	
	if (!og) {
		warn("Skipping node '%s', because this server is not running as an OPAL Async process!", n->name);
		return -1;
	}
	
	struct opal *o = alloc(sizeof(struct opal));
	
	config_setting_lookup_int(cfg, "send_id", &o->send_id);
	config_setting_lookup_int(cfg, "recv_id", &o->recv_id);
	config_setting_lookup_bool(cfg, "reply", &o->reply);
		
	/* Search for valid send and recv ids */
	int sfound = 0, rfound = 0;
	for (int i=0; i<og->send_icons; i++)
		sfound += og->send_ids[i] == o->send_id;
	for (int i=0; i<og->send_icons; i++)
		rfound += og->send_ids[i] == o->send_id;
	
	if (!sfound)
		cerror(config_setting_get_member(cfg, "send_id"),
			"Invalid send_id '%u' for node '%s'", o->send_id, n->name);
	if (!rfound)
		cerror(config_setting_get_member(cfg, "recv_id"),
			"Invalid recv_id '%u' for node '%s'", o->recv_id, n->name);

	n->opal = o;
	n->opal->global = og;
	n->cfg = cfg;

	return 0;
}

int opal_print(struct node *n, char *buf, int len)
{
	struct opal *o = n->opal;
	
	/** @todo: Print send_params, recv_params */
	
	return snprintf(buf, len, "send_id=%u, recv_id=%u, reply=%u",
		o->send_id, o->recv_id, o->reply);
}

int opal_open(struct node *n)
{
	struct opal *o = n->opal;
	
	OpalGetAsyncSendIconMode(&o->mode, o->send_id);
	OpalGetAsyncSendParameters(&o->send_params, sizeof(Opal_SendAsyncParam), o->send_id);
	OpalGetAsyncRecvParameters(&o->recv_params, sizeof(Opal_RecvAsyncParam), o->recv_id);

	return 0;
}

int opal_close(struct node *n)
{
	return 0;
}

int opal_read(struct node *n, struct msg *m)
{
	struct opal *o = n->opal;
	
	int state, len, ret;
	unsigned id;
	
	double data[MSG_VALUES];		
	
	/* This call unblocks when the 'Data Ready' line of a send icon is asserted. */
	do {
		if ((ret = OpalWaitForAsyncSendRequest(&id)) != EOK) {
			state = OpalGetAsyncModelState();
			if ((state == STATE_RESET) || (state == STATE_STOP)) {
				warn("OpalGetAsyncModelState(): Model stopped or resetted!");
				die();
			}

			return -1; // FIXME: correct return value
		}
	} while (id != o->send_id);

	/* No errors encountered yet */
	OpalSetAsyncSendIconError(0, o->send_id);

	/* Get the size of the data being sent by the unblocking SendID */
	OpalGetAsyncSendIconDataLength(&len, o->send_id);
	if (len > sizeof(data)) {
		warn("Ignoring the last %u of %u values for OPAL node '%s' (send_id=%u).",
		len / sizeof(double) - MSG_VALUES, len / sizeof(double), n->name, o->send_id);

		len = sizeof(data);
	}

	/* Read data from the model */
	OpalGetAsyncSendIconData(data, len, o->send_id);

	m->sequence = htons(o->seq_no++);
	m->length = len / sizeof(double);

	for (int i = 0; i < m->length; i++)
		m->data[i].f = (float) data[i]; // casting to float!

	/* This next call allows the execution of the "asynchronous" process
	 * to actually be synchronous with the model. To achieve this, you
	 * should set the "Sending Mode" in the Async_Send block to
	 * NEED_REPLY_BEFORE_NEXT_SEND or NEED_REPLY_NOW. This will force
	 * the model to wait for this process to call this
	 * OpalAsyncSendRequestDone function before continuing. */
	if (o->reply)
		OpalAsyncSendRequestDone(o->send_id);

	/* Before continuing, we make sure that the real-time model
	 * has not been stopped. If it has, we quit. */
	state = OpalGetAsyncModelState();
	if ((state == STATE_RESET) || (state == STATE_STOP)) {
		warn("OpalGetAsyncModelState(): Model stopped or resetted!");
		die();
	}

	return 0;
}

int opal_write(struct node *n, struct msg *m)
{
	struct opal *o = n->opal;
	
	int state;
	int len;
	
	double data[MSG_VALUES] = { NAN };
	
	state = OpalGetAsyncModelState();
	if ((state == STATE_RESET) || (state == STATE_STOP)) {
		warn("OpalGetAsyncModelState(): Model stopped or resetted!");
		die();
	}

	OpalSetAsyncRecvIconStatus(m->sequence, o->recv_id);	/* Set the Status to the message ID */
	OpalSetAsyncRecvIconError(0, o->recv_id);		/* Set the Error to 0 */

	/* Get the number of signals to send back to the model */
	OpalGetAsyncRecvIconDataLength(&len, o->recv_id);
	if (len > sizeof(data))
		error("Receive Block of OPAL node '%s' is expecting more signals than");
		
	for (int i = 0; i < m->length; i++)
		data[i] = (double) m->data[i].f;

	OpalSetAsyncRecvIconData(data, len, o->recv_id);
	
	return 0;
}
