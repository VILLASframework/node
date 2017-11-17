/** Node type: IEC 61850-9-2 (Sampled Values)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <libiec61850/stack_config.h>

#if CONFIG_IEC61850_SAMPLED_VALUES_SUPPORT == 1

#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/ether.h>

#include "nodes/iec61850_sv.h"
#include "plugin.h"

#define CONFIG_SV_DEFAULT_APPID 0x4000
#define CONFIG_SV_DEFAULT_DST_ADDRESS CONFIG_GOOSE_DEFAULT_DST_ADDRESS
#define CONFIG_SV_DEFAULT_PRIORITY 4
#define CONFIG_SV_DEFAULT_VLAN_ID 0

struct iec61850_type_descriptor type_descriptors[] = {
	/* name,              type,                        size, supported */
	{ "boolean",          IEC61850_TYPE_BOOLEAN,          1, false, false },
	{ "int8",             IEC61850_TYPE_INT8,             1, false, false },
	{ "int16",            IEC61850_TYPE_INT16,            2, false, false },
	{ "int32",            IEC61850_TYPE_INT32,            4, false, false },
	{ "int64",            IEC61850_TYPE_INT64,            8, false, false },
	{ "int8u",            IEC61850_TYPE_INT8U,            1, false, false },
	{ "int16u",           IEC61850_TYPE_INT16U,           2, false, false },
	{ "int32u",           IEC61850_TYPE_INT32U,           4, false, false },
	{ "int64u",           IEC61850_TYPE_INT64U,           8, false, false },
	{ "float32",          IEC61850_TYPE_FLOAT32,          4, false, false },
	{ "float64",          IEC61850_TYPE_FLOAT64,          8, false, false },
	{ "enumerated",       IEC61850_TYPE_ENUMERATED,       4, false, false },
	{ "coded_enum",       IEC61850_TYPE_CODED_ENUM,       4, false, false },
	{ "octet_string",     IEC61850_TYPE_OCTET_STRING,    20, false, false },
	{ "visible_string",   IEC61850_TYPE_VISIBLE_STRING,  35, false, false },
	{ "objectname",       IEC61850_TYPE_OBJECTNAME,      20, false, false },
	{ "objectreference",  IEC61850_TYPE_OBJECTREFERENCE, 20, false, false },
	{ "timestamp",        IEC61850_TYPE_TIMESTAMP,        8, false, false },
	{ "entrytime",        IEC61850_TYPE_ENTRYTIME,        6, false, false },
	{ "bitstring",        IEC61850_TYPE_BITSTRING,        4, false, false }
};

/** Each network interface needs a separate receiver */
static struct list receivers = LIST_INIT();
static pthread_t thread;

static void * iec61850_sv_thread(void *ctx)
{
	while (1) {
		for (unsigned i = 0; i < list_length(&receivers); i++) {
			struct iec61850_sv_receiver *r = (struct iec61850_sv_receiver *) list_at(&receivers, i);

			SVReceiver_tick(r->receiver); /* calls iec61850_sv_listener() */
		}

		usleep(1e3);
	}

	return NULL;
}

static void iec61850_sv_listener(SVSubscriber subscriber, void *ctx, SVSubscriber_ASDU asdu)
{
	struct node *n = (struct node *) ctx;
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;
	struct sample *smp;

	const char* svid = SVSubscriber_ASDU_getSvId(asdu);
	int smpcnt       = SVSubscriber_ASDU_getSmpCnt(asdu);
	int confrev      = SVSubscriber_ASDU_getConfRev(asdu);

	debug(5, "Received SV: svid=%s, smpcnt=%i, confrev=%u", svid, smpcnt, confrev);

	if (SVSubscriber_ASDU_getDataSize(asdu) < i->subscriber.total_size) {
		warn("Received truncated ASDU: size=%d, expected=%d", SVSubscriber_ASDU_getDataSize(asdu), i->subscriber.total_size);
		return;
	}

	/* Access to the data requires a priori knowledge of the data set.
	 * For this example we assume a data set consisting of FLOAT32 values.
	 * A FLOAT32 value is encoded as 4 bytes. You can find the first FLOAT32
	 * value at byte position 0, the second value at byte position 4, the third
	 * value at byte position 8, and so on.
	 *
	 * To prevent damages due configuration, please check the length of the
	 * data block of the SV message before accessing the data.
	 */

	smp = pool_get(&i->subscriber.pool);
	if (!smp) {
		warn("Pool underrun in subscriber of %s", node_name(n));
		return;
	}

	smp->sequence = smpcnt;

	smp->flags = SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_VALUES;
	smp->length = 0;

	if (SVSubscriber_ASDU_hasRefrTm(asdu)) {
		uint64_t refrtm = SVSubscriber_ASDU_getRefrTmAsMs(asdu);

		smp->ts.origin.tv_sec = refrtm / 1000;
		smp->ts.origin.tv_nsec = (refrtm % 1000) * 1000000;
		smp->flags |= SAMPLE_HAS_ORIGIN;

		info("refr read: %zu", refrtm);
	}

	unsigned offset = 0;
	for (size_t j = 0; j < list_length(&i->subscriber.mapping); j++) {
		struct iec61850_type_descriptor *td = (struct iec61850_type_descriptor *) list_at(&i->subscriber.mapping, j);

		switch (td->type) {
			case IEC61850_TYPE_INT8:
			case IEC61850_TYPE_INT16:
			case IEC61850_TYPE_INT32:
			case IEC61850_TYPE_INT8U:
			case IEC61850_TYPE_INT16U:
			case IEC61850_TYPE_INT32U:
				sample_set_data_format(smp, j, SAMPLE_DATA_FORMAT_INT);
				break;

			case IEC61850_TYPE_FLOAT32:
			case IEC61850_TYPE_FLOAT64:
				sample_set_data_format(smp, j, SAMPLE_DATA_FORMAT_FLOAT);
				break;
			default: { }
		}

		switch (td->type) {
			case IEC61850_TYPE_INT8:    smp->data[j].i = SVSubscriber_ASDU_getINT8(asdu,    offset); break;
			case IEC61850_TYPE_INT16:   smp->data[j].i = SVSubscriber_ASDU_getINT16(asdu,   offset); break;
			case IEC61850_TYPE_INT32:   smp->data[j].i = SVSubscriber_ASDU_getINT32(asdu,   offset); break;
			case IEC61850_TYPE_INT8U:   smp->data[j].i = SVSubscriber_ASDU_getINT8U(asdu,   offset); break;
			case IEC61850_TYPE_INT16U:  smp->data[j].i = SVSubscriber_ASDU_getINT16U(asdu,  offset); break;
			case IEC61850_TYPE_INT32U:  smp->data[j].i = SVSubscriber_ASDU_getINT32U(asdu,  offset); break;
			case IEC61850_TYPE_FLOAT32: smp->data[j].f = SVSubscriber_ASDU_getFLOAT32(asdu, offset); break;
			case IEC61850_TYPE_FLOAT64: smp->data[j].f = SVSubscriber_ASDU_getFLOAT64(asdu, offset); break;
			default: { }
		}

		offset += td->size;

		smp->length++;
	}

	queue_signalled_push(&i->subscriber.queue, smp);
}

static struct iec61850_type_descriptor * iec61850_lookup_type(const char *name)
{
	for (unsigned i = 0; i < ARRAY_LEN(type_descriptors); i++) {
		if (!strcmp(name, type_descriptors[i].name))
			return &type_descriptors[i];
	}

	return NULL;
}

static int iec61850_sv_parse_mapping(json_t *json_mapping, struct list *mapping)
{
	json_t *json_field;
	size_t index;

	if (!json_is_array(json_mapping))
		return -1;

	list_init(mapping);

	int total_size = 0;
	json_array_foreach(json_mapping, index, json_field) {
		struct iec61850_type_descriptor *m;
		const char *type = json_string_value(json_field);

		if (!json_is_string(json_field))
			return -1;

		m = iec61850_lookup_type(type);
		if (!m)
			return -1;

		list_push(mapping, m);

		total_size += m->size;
	}

	return total_size;
}

int iec61850_sv_init(struct super_node *sn)
{
	int ret;

	/* Start all receivers */
	for (size_t i = 0; i < list_length(&receivers); i++) {
		struct iec61850_sv_receiver *r = (struct iec61850_sv_receiver *) list_at(&receivers, i);

		SVReceiver_start(r->receiver);
	}

	ret = pthread_create(&thread, NULL, iec61850_sv_thread, NULL);
	if (ret)
		return ret;

	return 0;
}

int iec61850_sv_deinit()
{
	int ret;

	ret = pthread_cancel(thread);
	if (ret)
		return ret;

	ret = pthread_join(thread, NULL);
	if (ret)
		return ret;

	for (size_t i = 0; i < list_length(&receivers); i++) {
		struct iec61850_sv_receiver *r = (struct iec61850_sv_receiver *) list_at(&receivers, i);

		/* Stop all receivers */
		SVReceiver_stop(r->receiver);

		/* Cleanup and free resources */
		SVReceiver_destroy(r->receiver);
	}

	list_destroy(&receivers, NULL, false);

	return 0;
}

int iec61850_sv_parse(struct node *n, json_t *json)
{
	int ret;
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;

	const char *dst_address = NULL;
	const char *interface = NULL;
	const char *svid = NULL;
	const char *datset = NULL;
	const char *smpmod = NULL;

	json_t *json_sub = NULL;
	json_t *json_pub = NULL;
	json_t *json_mapping = NULL;
	json_error_t err;

	/* Default values */
	i->publisher.confrev = 1;
	i->publisher.vlan_priority = CONFIG_SV_DEFAULT_PRIORITY;
	i->publisher.vlan_id = CONFIG_SV_DEFAULT_VLAN_ID;

	i->app_id = CONFIG_SV_DEFAULT_APPID;

	uint8_t tmp[] = CONFIG_SV_DEFAULT_DST_ADDRESS;
	memcpy(i->dst_address.ether_addr_octet, tmp, sizeof(i->dst_address.ether_addr_octet));

	ret = json_unpack_ex(json, &err, 0, "{ s?: o, s?: o, s: s, s?: i, s?: s }",
		"publish", &json_pub,
		"subscribe", &json_sub,
		"interface", &interface,
		"app_id", &i->app_id,
		"dst_address", &dst_address
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (interface)
		i->interface = strdup(interface);

	if (dst_address)
		ether_aton_r(dst_address, &i->dst_address);

	if (json_pub) {
		ret = json_unpack_ex(json_pub, &err, 0, "{ s: o, s?: s, s?: s, s?: i, s: s, s?: i, s?: i }",
			"fields", &json_mapping,
			"svid", &svid,
			"datset", &datset,
			"confrev", &i->publisher.confrev,
			"smpmod", &smpmod,
			"vlan_id", &i->publisher.vlan_id,
			"vlan_priority", &i->publisher.vlan_priority
		);
		if (ret)
			jerror(&err, "Failed to parse configuration of node %s", node_name(n));

		if (smpmod) {
			if      (!strcmp(smpmod, "per_nominal_period"))
				i->publisher.smpmod = IEC61850_SV_SMPMOD_PER_NOMINAL_PERIOD;
			else if (!strcmp(smpmod, "samples_per_second"))
				i->publisher.smpmod = IEC61850_SV_SMPMOD_SAMPLES_PER_SECOND;
			else if (!strcmp(smpmod, "seconds_per_sample"))
				i->publisher.smpmod = IEC61850_SV_SMPMOD_SECONDS_PER_SAMPLE;
			else
				error("Invalid value '%s' for setting 'smpmod'", smpmod);
		}

		i->publisher.svid = svid ? strdup(svid) : NULL;
		i->publisher.datset = datset ? strdup(datset) : NULL;

		ret = iec61850_sv_parse_mapping(json_mapping, &i->publisher.mapping);
		if (ret <= 0)
			error("Failed to parse setting 'fields' of node %s", node_name(n));

		i->publisher.total_size = ret;
	}

	if (json_sub) {
		ret = json_unpack_ex(json_sub, &err, 0, "{ s: o }",
			"fields", &json_mapping
		);
		if (ret)
			jerror(&err, "Failed to parse configuration of node %s", node_name(n));

		ret = iec61850_sv_parse_mapping(json_mapping, &i->subscriber.mapping);
		if (ret <= 0)
			error("Failed to parse setting 'fields' of node %s", node_name(n));

		i->subscriber.total_size = ret;

		/* Check if there is already a SVReceiver for this interface */
		struct iec61850_sv_receiver *r = (struct iec61850_sv_receiver *) list_lookup(&receivers, interface);

		if (!r) {
			r = alloc(sizeof(struct iec61850_sv_receiver));
			if (!r)
				return -1;

			r->interface = strdup(interface);
			r->receiver = SVReceiver_create();

			SVReceiver_setInterfaceId(r->receiver, r->interface);

			list_push(&receivers, r);
		}

		i->subscriber.receiver = r->receiver;
	}

	return 0;
}

char * iec61850_sv_print(struct node *n)
{
	char *buf;
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;

	buf = strf("interface=%s, app_id=%#x, dst_address=%s", i->interface, i->app_id, ether_ntoa(&i->dst_address));

	/* Publisher part */
	strcatf(&buf, ", pub.vlan_prio=%d, pub.vlan_id=%#x, pub.confrev=%d, sub.#fields=%zu",
		i->publisher.vlan_priority,
		i->publisher.vlan_id,
		i->publisher.confrev,
		list_length(&i->publisher.mapping)
	);

	if (i->publisher.datset)
		strcatf(&buf, ", pub.datset=%s", i->publisher.datset);

	/* Subscriber part */
	strcatf(&buf, ", sub.#fields=%zu", list_length(&i->subscriber.mapping));

	return buf;
}

int iec61850_sv_start(struct node *n)
{
	int ret;
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;

	/* Initialize publisher */
	i->publisher.publisher = SVPublisher_create(NULL, i->interface);
	i->publisher.asdu = SVPublisher_addASDU(i->publisher.publisher, i->publisher.svid, i->publisher.datset, i->publisher.confrev);

	for (unsigned k = 0; k < list_length(&i->publisher.mapping); k++) {
		struct iec61850_type_descriptor *m = (struct iec61850_type_descriptor *) list_at(&i->publisher.mapping, k);

		switch (m->type) {
			case IEC61850_TYPE_INT8:    SV_ASDU_addINT8(i->publisher.asdu); break;
			case IEC61850_TYPE_INT32:   SV_ASDU_addINT32(i->publisher.asdu); break;
			case IEC61850_TYPE_FLOAT32: SV_ASDU_addFLOAT(i->publisher.asdu); break;
			case IEC61850_TYPE_FLOAT64: SV_ASDU_addFLOAT64(i->publisher.asdu); break;
			default: { }
		}
	}

	SVPublisher_setupComplete(i->publisher.publisher);

	/* Initialize subscriber */
	i->subscriber.subscriber = SVSubscriber_create(i->dst_address.ether_addr_octet, i->app_id);

	/* Install a callback handler for the subscriber */
	SVSubscriber_setListener(i->subscriber.subscriber, iec61850_sv_listener, n);

	/* Connect the subscriber to the receiver */
	SVReceiver_addSubscriber(i->subscriber.receiver, i->subscriber.subscriber);

	/* Initialize pool and queue to pass samples between threads */
	ret = pool_init(&i->subscriber.pool, 1024, SAMPLE_LEN(n->samplelen), &memtype_hugepage);
	if (ret)
		return ret;

	ret = queue_signalled_init(&i->subscriber.queue, 1024, &memtype_hugepage, 0);
	if (ret)
		return ret;

	return 0;
}

int iec61850_sv_stop(struct node *n)
{
	int ret;
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;

	/* Deinitialize publisher */
	SVPublisher_destroy(i->publisher.publisher);

	/* Deinitialise subscriber */
	ret = queue_signalled_destroy(&i->subscriber.queue);
	if (ret)
		return ret;

	ret = pool_destroy(&i->subscriber.pool);
	if (ret)
		return ret;

	return 0;
}

int iec61850_sv_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int pulled;
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;
	struct sample *smpt[cnt];

	pulled = queue_signalled_pull_many(&i->subscriber.queue, (void **) smpt, cnt);

	sample_copy_many(smps, smpt, pulled);

	return pulled;
}

int iec61850_sv_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;

	for (unsigned j = 0; j < cnt; j++) {
		unsigned offset = 0;
		for (unsigned k = 0; k < MIN(smps[j]->length, list_length(&i->publisher.mapping)); k++) {
			struct iec61850_type_descriptor *m = (struct iec61850_type_descriptor *) list_at(&i->publisher.mapping, k);

			int ival = 0;
			double fval = 0;

			switch (m->type) {
				case IEC61850_TYPE_INT8:
				case IEC61850_TYPE_INT32:
					ival = sample_get_data_format(smps[j], k) == SAMPLE_DATA_FORMAT_FLOAT ? smps[j]->data[k].f : smps[j]->data[k].i;
					break;

				case IEC61850_TYPE_FLOAT32:
				case IEC61850_TYPE_FLOAT64:
					fval = sample_get_data_format(smps[j], k) == SAMPLE_DATA_FORMAT_FLOAT ? smps[j]->data[k].f : smps[j]->data[k].i;
					break;

				default: { }
			}

			switch (m->type) {
				case IEC61850_TYPE_INT8:    SVPublisher_ASDU_setINT8(i->publisher.asdu,    offset, ival); break;
				case IEC61850_TYPE_INT32:   SVPublisher_ASDU_setINT32(i->publisher.asdu,   offset, ival); break;
				case IEC61850_TYPE_FLOAT32: SVPublisher_ASDU_setFLOAT32(i->publisher.asdu, offset, fval); break;
				case IEC61850_TYPE_FLOAT64: SVPublisher_ASDU_setFLOAT64(i->publisher.asdu, offset, fval); break;
				default: { }
			}

			offset += m->size;
		}

		SVPublisher_ASDU_setSmpCnt(i->publisher.asdu, smps[j]->sequence);

		if (smps[j]->flags & SAMPLE_HAS_ORIGIN) {
			uint64_t refrtm = smps[j]->ts.origin.tv_sec * 1000 + smps[j]->ts.origin.tv_nsec / 1000000;

			info("refr write: %zu", refrtm);

			SVPublisher_ASDU_setRefrTm(i->publisher.asdu, refrtm);
		}

		SVPublisher_publish(i->publisher.publisher);
	}

	return cnt;
}

int iec61850_sv_fd(struct node *n)
{
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;

	return queue_signalled_fd(&i->subscriber.queue);
}

static struct plugin p = {
	.name		= "iec61850-9-2",
	.description	= "IEC 61850-9-2 (Sampled Values)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct iec61850_sv),
		.init		= iec61850_sv_init,
		.deinit		= iec61850_sv_deinit,
		.parse		= iec61850_sv_parse,
		.print		= iec61850_sv_print,
		.start		= iec61850_sv_start,
		.stop		= iec61850_sv_stop,
		.read		= iec61850_sv_read,
		.write		= iec61850_sv_write,
		.fd		= iec61850_sv_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)

#endif /* CONFIG_IEC61850_SAMPLED_VALUES_SUPPORT */
