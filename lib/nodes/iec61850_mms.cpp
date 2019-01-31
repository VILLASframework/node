/** Node type: IEC 61850-8-1 (MMS)
 *
 * @author Iris Koester <ikoester@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/nodes/iec61850_mms.hpp>
#include <villas/plugin.hpp>
#include <villas/log.hpp>

static int iec61850_mms_signal_destroy(struct iec61850_mms_signal *s)
{
	free(s->domain_id);
	free(s->item_id);

	return 0;
}

static int iec61850_mms_parse_signal(json_t *json_signal, struct iec61850_mms_signal *mms_sig, struct signal *sig)
{
	int ret = 0;
	const char *domain_id, *item_id;

	json_error_t err;
	ret = json_unpack_ex(json_signal, &err, 0, "{ s: s, s: s }",
		"domain", &domain_id,
		"item", &item_id
	);
	if (ret)
		warning("Failed to parse configuration while reading domain and item ids");

	mms_sig->domain_id = strdup(domain_id);
	mms_sig->item_id = strdup(item_id);
	mms_sig->type = iec61850_parse_signal(json_signal, sig);
	if (!sig->type)
		return -1;

	return 0;
}

/** Parse MMS configuration parameters
  *
  * @param mms_ids JSON object that contains pairs of domain and item IDs
  * @param signals pointer to list of type struct iec61850_mms_signal
  * @param node_signals pointer to list of type struct signal
  *
  * @return total size of data
  */
static int iec61850_mms_parse_signals(json_t *json_signals, struct vlist *signals, struct vlist *node_signals)
{
	int ret;

	size_t i = 0;
	json_t *json_signal;

	int total_size = 0;

	json_array_foreach(json_signals, i, json_signal) {
		struct signal *sig;
		struct iec61850_mms_signal *mms_sig;

		sig = vlist_at(node_signals, i);
		if (!sig)
			return -1;

		mms_sig = alloc(sizeof(struct iec61850_mms_signal));
		if (!sig)
			return -1;

		ret = iec61850_mms_parse_signal(json_signal, mms_sig, sig);
		if (ret) {
			free(sig);
			return -2;
		}

		total_size += mms_sig->type->size;

		vlist_push(signals, (void *) sig);
	}

	return total_size;
}

int iec61850_mms_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;

	const char *host = NULL;

	json_error_t err;
	json_t *json_in = NULL;
	json_t *json_out = NULL;
	json_t *json_signals = NULL;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: s, s: i, s: i, s?: o, s?: o }",
		"host", &host,
		"port", &mms->port,
		"rate", &mms->rate,
		"in", &json_in,
		"out", &json_out
	);

	mms->host = strdup(host);

	if (ret)
		jerror (&err, "Failed to parse configuration of node %s", node_name(n));

	if (json_in) {
		ret = json_unpack_ex(json_in, &err, 0, "{ s: o }",
			"signals", &json_signals
		);
		if (ret)
			jerror(&err, "Failed to parse configuration of node %s", node_name(n));

		ret = iec61850_mms_parse_signals(json_signals, &mms->in.signals, &n->signals);
		if (ret <= 0)
			error("Failed to parse setting 'signals' of node %s", node_name(n));
	}

	if (json_out) {
		ret = json_unpack_ex(json_out, &err, 0, "{ s?: b, s?: i, s: o }",
			"test", &mms->out.is_test,
			"testvalue", &mms->out.testvalue,
			"signals", &json_signals
		);
		if (ret)
			jerror(&err, "Failed to parse configuration of node %s", node_name(n));

		ret = iec61850_mms_parse_signals(json_signals, &mms->out.signals, &n->signals);
		if (ret <= 0)
			error("Failed to parse setting 'iecList' of node %s", node_name(n));
	}

	return 0;
}

char *iec61850_mms_print(struct node *n)
{
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;

	return strf("host=%s, port=%d", mms->host, mms->port);
}

// create connection to MMS server
int iec61850_mms_start(struct node *n)
{
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;

	mms->conn = MmsConnection_create();

	/* Connect to MMS Server */
	MmsError err;
	if (!MmsConnection_connect(mms->conn, &err, mms->host, mms->port))
		error("Cannot connect to MMS server: %s:%d", mms->host, mms->port);

	// setup task
	int ret = task_init(&mms->task, mms->rate, CLOCK_MONOTONIC);
	if (ret)
		return ret;

	return 0;
}

// destroy connection to MMS server
int iec61850_mms_stop(struct node *n)
{
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;

	MmsConnection_destroy(mms->conn); // doesn't have return value

	return 0;
}

// read from MMS Server
int iec61850_mms_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;
	struct sample *smp = smps[0];
	//struct timespec time;

	// read value from MMS server
	MmsError error;
	MmsValue *mms_val;

	task_wait(&mms->task);

	// TODO: timestamp von MMS server?
	//time = time_now();


	smp->flags = SAMPLE_HAS_DATA | SAMPLE_HAS_SEQUENCE;
	smp->length = 0;

	for (size_t j = 0; j < vlist_length(&mms->in.signals); j++) {
		struct iec61850_mms_signal *sig = vlist_at(&mms->in.signals, j);

		mms_val = MmsConnection_readVariable(mms->conn, &error, sig->domain_id, sig->item_id);
		if (mms_val == NULL)
			warning("Reading MMS value from server failed");

		// convert result according data type
		switch (sig->type->type) {
			case IEC61850_TYPE_INT32:
				smp->data[j].i = MmsValue_toInt32(mms_val);
				break;

			case IEC61850_TYPE_INT32U:
				smp->data[j].i = MmsValue_toUint32(mms_val);
				break;

			case IEC61850_TYPE_FLOAT32:
				smp->data[j].f = MmsValue_toFloat(mms_val);
				break;

			case IEC61850_TYPE_FLOAT64:
				smp->data[j].f = MmsValue_toDouble(mms_val);
				break;

			default: { }
		}

		smp->length++;
	}

	MmsValue_delete(mms_val);

	return 1;
}

// TODO
int iec61850_mms_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
//	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;

	return 0;
}

int iec61850_mms_init(struct node *n)
{
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;
	int ret;

	ret = vlist_init(&mms->in.signals);
	if (ret)
		return ret;

	ret = vlist_init(&mms->out.signals);
	if (ret)
		return ret;

	return 0;
}

int iec61850_mms_destroy(struct node *n)
{
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;
	int ret;

	ret = vlist_destroy(&mms->in.signals, (dtor_cb_t) iec61850_mms_signal_destroy, true);
	if (ret)
		return ret;

	ret = vlist_destroy(&mms->out.signals, (dtor_cb_t) iec61850_mms_signal_destroy, true);
	if (ret)
		return ret;

	MmsConnection_destroy(mms->conn);

	free(mms->host);

	return 0;
}

int iec61850_mms_poll_fds(struct node *n, int fds[])
{
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;

	fds[0] = task_fd(&mms->task);

	return 1;
}


static struct plugin p = {
	.name		= "iec61850-mms",
	.description	= "IEC 61850-8-1 MMS (libiec61850)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0, /* unlimited */
		.size		= sizeof(struct iec61850_mms),
		.type.start	= iec61850_type_start,	// TODO
		.type.stop	= iec61850_type_stop,	// TODO
		.parse		= iec61850_mms_parse,
		.print		= iec61850_mms_print,
		.start		= iec61850_mms_start,
		.stop		= iec61850_mms_stop,	// TODO
		.destroy	= iec61850_mms_destroy,
		.read		= iec61850_mms_read,
		.write		= iec61850_mms_write,	// TODO: Rumpf erstellt
		.poll_fds	= iec61850_mms_poll_fds
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
