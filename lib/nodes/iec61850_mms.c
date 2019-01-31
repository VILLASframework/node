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

#include <villas/nodes/iec61850_mms.h>
#include <villas/plugin.h>
#include <villas/log.h>


int iec61850_mms_parse_ids(json_t *mms_ids, struct vlist *domain_ids, struct vlist *item_ids)
{
	int ret = 0, totalsize = 0;
	const char *domain_id;
	const char *item_id;

	ret = vlist_init(domain_ids);
	if (ret)
		return ret;

	ret = vlist_init(item_ids);
	if (ret)
		return ret;

	size_t i = 0;
	json_t *mms_id;
	json_error_t err;
	json_array_foreach(mms_ids, i, mms_id) {

		ret = json_unpack_ex(mms_id, &err, 0, "{s: s, s: s}",
					"domain_ID", &domain_id,
					"item_ID", &item_id
					);

		if (ret)
				warn("Failed to parse configuration while reading domain and item ids");


		vlist_push(domain_ids, (void *) domain_id);
		vlist_push(item_ids, (void *) item_id);

		totalsize++;
	}
	return totalsize;
}

int iec61850_mms_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;

	json_error_t err;

	const char *host = NULL;
	json_t *json_in = NULL;
	json_t *json_out = NULL;
	json_t *json_signals = NULL;
	json_t *json_mms_ids = NULL;

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

		ret = json_unpack_ex(json_in, &err, 0, "{ s: o, s: o }",
					"iec_types", &json_signals,
					"mms_ids", &json_mms_ids
					);
		if (ret)
				jerror(&err, "Failed to parse configuration of node %s", node_name(n));

		ret = iec61850_parse_signals(json_signals, &mms->in.iec_type_list, &n->signals);
		if (ret <= 0)
				error("Failed to parse setting 'signals' of node %s", node_name(n));

		mms->in.totalsize = ret;

		int length_mms_ids = iec61850_mms_parse_ids(json_mms_ids, &mms->in.domain_ids, &mms->in.item_ids);

		int length_iec_types = mms->in.iec_type_list.length;
		if (length_mms_ids == -1)
				error("Configuration error in node '%s': json error while parsing", node_name(n));
		else if (length_iec_types != length_mms_ids)  // length of the lists is not the same
				error("Configuration error in node '%s': one set of 'mms_ids'(%d value(s)) should match one value of 'iecTypes'(%d value(s))", node_name(n), length_mms_ids, length_iec_types);
	}

	if (json_out) {
		ret = json_unpack_ex(json_out, &err, 0, "{ s?: b, s?: i, s: o, s: o }",
					"test", &mms->out.is_test,
					"testvalue", &mms->out.testvalue,
					"iec_types", &json_signals,
					"mms_ids", &json_mms_ids
					);
		if (ret)
				jerror(&err, "Failed to parse configuration of node %s", node_name(n));

		ret = iec61850_parse_signals(json_signals, &mms->out.iec_type_list, &n->signals);
		if (ret <= 0)
				error("Failed to parse setting 'iecList' of node %s", node_name(n));

		mms->out.totalsize = ret;

		int length_mms_ids = iec61850_mms_parse_ids(json_mms_ids, &mms->out.domain_ids, &mms->out.item_ids);

		int length_iec_types = mms->out.iec_type_list.length;
		if (length_mms_ids == -1)
				error("Configuration error in node '%s': json error while parsing", node_name(n));
		else if (length_iec_types != length_mms_ids)  // length of the lists is not the same
				error("Configuration error in node '%s': one set of 'mms_ids'(%d value(s)) should match one value of 'iecTypes'(%d value(s))", node_name(n), length_mms_ids, length_iec_types);
	}

	return 0;
}

char *iec61850_mms_print(struct node *n)
{
	char *buf;
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;

	buf = strf("host = %s, port = %d, domain_id = %s, item_id = %s", mms->host, mms->port, mms->domain_id, mms->item_id);

	return buf;
}

// create connection to MMS server
int iec61850_mms_start(struct node *n)
{
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;

	mms->conn = MmsConnection_create();

	/* Connect to MMS Server */
	MmsError err;
	if (!MmsConnection_connect(mms->conn, &err, mms->host, mms->port))
	{
		error("Cannot connect to MMS server: %s:%d", mms->host, mms->port);
	}

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

	const char *domain_id;
	const char *item_id;

	for (size_t j = 0; j < vlist_length(&mms->in.iec_type_list); j++)
	{
		// get MMS Value from server
		domain_id = (const char *) vlist_at(&mms->in.domain_ids, j);
		item_id = (const char *) vlist_at(&mms->in.item_ids, j);

		mms_val = MmsConnection_readVariable(mms->conn, &error, domain_id, item_id);

		if (mms_val == NULL) {
				warn("Reading MMS value from server failed");
		}

		// convert result according data type
		struct iec61850_type_descriptor *td = (struct iec61850_type_descriptor *) vlist_at(&mms->in.iec_type_list, j);

		switch (td->type) {
				case IEC61850_TYPE_INT32:	smp->data[j].i = MmsValue_toInt32(mms_val); break;
				case IEC61850_TYPE_INT32U:  smp->data[j].i = MmsValue_toUint32(mms_val); break;
				case IEC61850_TYPE_FLOAT32: smp->data[j].f = MmsValue_toFloat(mms_val); break;
				case IEC61850_TYPE_FLOAT64: smp->data[j].f = MmsValue_toDouble(mms_val); break;
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

int iec61850_mms_destroy(struct node *n)
{
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;

	MmsConnection_destroy(mms->conn);

	free(mms->host);
	free(mms->domain_id);
	free(mms->item_id);

	return 0;
}

int iec61850_mms_fd(struct node *n)
{
	struct iec61850_mms *mms = (struct iec61850_mms *) n->_vd;

	return task_fd(&mms->task);
}


static struct plugin p = {
	.name		= "iec61850-8-1",
	.description	= "IEC 61850-8-1 MMS (libiec61850)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0, /* unlimited */
		.size		= sizeof(struct iec61850_mms),
		.type.start	= iec61850_type_start,	// TODO
		.type.stop	= iec61850_type_stop,	// TODO
		.parse		= iec61850_mms_parse,	/* done */
		.print		= iec61850_mms_print,	/* done */
		.start		= iec61850_mms_start,	/* done */
		.stop		= iec61850_mms_stop,	// TODO
		.destroy	= iec61850_mms_destroy,	/* done */
		.read		= iec61850_mms_read,	/* done */
		.write		= iec61850_mms_write,	// TODO: Rumpf erstellt
		.fd		= iec61850_mms_fd
	}
};

REGISTER_PLUGIN(&p);
vlist_INIT_STATIC(&p.node.instances);
