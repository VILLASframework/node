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

#include "villas/nodes/iec61850_sv.h"
#include "villas/plugin.h"

#define CONFIG_SV_DEFAULT_APPID 0x4000
#define CONFIG_SV_DEFAULT_DST_ADDRESS CONFIG_GOOSE_DEFAULT_DST_ADDRESS
#define CONFIG_SV_DEFAULT_PRIORITY 4
#define CONFIG_SV_DEFAULT_VLAN_ID 0

static void iec61850_sv_listener(SVSubscriber subscriber, void *ctx, SVSubscriber_ASDU asdu)
{
	struct node *n = (struct node *) ctx;
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;
	struct sample *smp;

	const char* svid = SVSubscriber_ASDU_getSvId(asdu);
	int smpcnt       = SVSubscriber_ASDU_getSmpCnt(asdu);
	int confrev      = SVSubscriber_ASDU_getConfRev(asdu);

	debug(10, "Received SV: svid=%s, smpcnt=%i, confrev=%u", svid, smpcnt, confrev);

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

	smp = sample_alloc(&i->subscriber.pool);
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

int iec61850_sv_parse(struct node *n, json_t *json)
{
	int ret;
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;

	const char *dst_address = NULL;
	const char *interface = NULL;
	const char *svid = NULL;
	const char *smpmod = NULL;

	json_t *json_sub = NULL;
	json_t *json_pub = NULL;
	json_t *json_mapping = NULL;
	json_error_t err;

	/* Default values */
	i->publisher.enabled = false;
	i->subscriber.enabled = false;
	i->publisher.smpmod = -1; /* do not set smpmod */
	i->publisher.smprate = -1; /* do not set smpmod */
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

	if (dst_address) {
#ifdef __APPLE__
		struct ether_addr *ether = ether_aton(dst_address);
		memcpy(&i->dst_address, ether, sizeof(struct ether_addr));
#else
		ether_aton_r(dst_address, &i->dst_address);
#endif
	}

	if (json_pub) {
		i->publisher.enabled = true;

		ret = json_unpack_ex(json_pub, &err, 0, "{ s: o, s: s, s?: i, s?: s, s?: i, s?: i, s?: i }",
			"fields", &json_mapping,
			"svid", &svid,
			"confrev", &i->publisher.confrev,
			"smpmod", &smpmod,
			"smprate", &i->publisher.smprate,
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

		ret = iec61850_parse_mapping(json_mapping, &i->publisher.mapping);
		if (ret <= 0)
			error("Failed to parse setting 'fields' of node %s", node_name(n));

		i->publisher.total_size = ret;
	}

	if (json_sub) {
		i->subscriber.enabled = true;

		ret = json_unpack_ex(json_sub, &err, 0, "{ s: o }",
			"fields", &json_mapping
		);
		if (ret)
			jerror(&err, "Failed to parse configuration of node %s", node_name(n));

		ret = iec61850_parse_mapping(json_mapping, &i->subscriber.mapping);
		if (ret <= 0)
			error("Failed to parse setting 'fields' of node %s", node_name(n));

		i->subscriber.total_size = ret;
	}

	return 0;
}

char * iec61850_sv_print(struct node *n)
{
	char *buf;
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;

	buf = strf("interface=%s, app_id=%#x, dst_address=%s", i->interface, i->app_id, ether_ntoa(&i->dst_address));

	/* Publisher part */
	if (i->publisher.enabled) {
		strcatf(&buf, ", pub.svid=%s, pub.vlan_prio=%d, pub.vlan_id=%#x, pub.confrev=%d, pub.#fields=%zu",
			i->publisher.svid,
			i->publisher.vlan_priority,
			i->publisher.vlan_id,
			i->publisher.confrev,
			list_length(&i->publisher.mapping)
		);
	}

	/* Subscriber part */
	if (i->subscriber.enabled)
		strcatf(&buf, ", sub.#fields=%zu", list_length(&i->subscriber.mapping));

	return buf;
}

int iec61850_sv_start(struct node *n)
{
	int ret;
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;

	/* Initialize publisher */
	if (i->publisher.enabled) {
		i->publisher.publisher = SVPublisher_create(NULL, i->interface);
		i->publisher.asdu = SVPublisher_addASDU(i->publisher.publisher, i->publisher.svid, node_name_short(n), i->publisher.confrev);

		for (unsigned k = 0; k < list_length(&i->publisher.mapping); k++) {
			struct iec61850_type_descriptor *m = (struct iec61850_type_descriptor *) list_at(&i->publisher.mapping, k);

			switch (m->type) {
				case IEC61850_TYPE_INT8:    SVPublisher_ASDU_addINT8(i->publisher.asdu); break;
				case IEC61850_TYPE_INT32:   SVPublisher_ASDU_addINT32(i->publisher.asdu); break;
				case IEC61850_TYPE_FLOAT32: SVPublisher_ASDU_addFLOAT(i->publisher.asdu); break;
				case IEC61850_TYPE_FLOAT64: SVPublisher_ASDU_addFLOAT64(i->publisher.asdu); break;
				default: { }
			}
		}

		if (i->publisher.smpmod >= 0)
			SVPublisher_ASDU_setSmpMod(i->publisher.asdu, i->publisher.smpmod);

//		if (s->publisher.smprate >= 0)
//			SV_ASDU_setSmpRate(i->publisher.asdu, i->publisher.smprate);

		/* Start publisher */
		SVPublisher_setupComplete(i->publisher.publisher);
	}

	/* Start subscriber */
	if (i->subscriber.enabled) {
		struct iec61850_receiver *r = iec61850_receiver_create(IEC61850_RECEIVER_SV, i->interface);

		i->subscriber.receiver = r->sv;
		i->subscriber.subscriber = SVSubscriber_create(i->dst_address.ether_addr_octet, i->app_id);

		/* Install a callback handler for the subscriber */
		SVSubscriber_setListener(i->subscriber.subscriber, iec61850_sv_listener, n);

		/* Connect the subscriber to the receiver */
		SVReceiver_addSubscriber(i->subscriber.receiver, i->subscriber.subscriber);

		/* Initialize pool and queue to pass samples between threads */
		ret = pool_init(&i->subscriber.pool, 1024, SAMPLE_LEN(n->samplelen), &memory_hugepage);
		if (ret)
			return ret;

		ret = queue_signalled_init(&i->subscriber.queue, 1024, &memory_hugepage, 0);
		if (ret)
			return ret;
	}

	return 0;
}

int iec61850_sv_stop(struct node *n)
{
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;

	if (i->subscriber.enabled)
		SVReceiver_removeSubscriber(i->subscriber.receiver, i->subscriber.subscriber);

	return 0;
}

int iec61850_sv_destroy(struct node *n)
{
	int ret;
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;

	/* Deinitialize publisher */
	if (i->publisher.enabled && i->publisher.publisher)
		SVPublisher_destroy(i->publisher.publisher);

	/* Deinitialise subscriber */
	if (i->subscriber.enabled) {
		ret = queue_signalled_destroy(&i->subscriber.queue);
		if (ret)
			return ret;

		ret = pool_destroy(&i->subscriber.pool);
		if (ret)
			return ret;
	}

	return 0;
}

int iec61850_sv_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *released)
{
	int pulled;
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;
	struct sample *smpt[cnt];

	if (!i->subscriber.enabled)
		return 0;

	pulled = queue_signalled_pull_many(&i->subscriber.queue, (void **) smpt, cnt);

	sample_copy_many(smps, smpt, pulled);
	sample_put_many(smpt, pulled);

	return pulled;
}

int iec61850_sv_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *released)
{
	struct iec61850_sv *i = (struct iec61850_sv *) n->_vd;

	if (!i->publisher.enabled)
		return 0;

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
				case IEC61850_TYPE_FLOAT32: SVPublisher_ASDU_setFLOAT(i->publisher.asdu,   offset, fval); break;
				case IEC61850_TYPE_FLOAT64: SVPublisher_ASDU_setFLOAT64(i->publisher.asdu, offset, fval); break;
				default: { }
			}

			offset += m->size;
		}

		SVPublisher_ASDU_setSmpCnt(i->publisher.asdu, smps[j]->sequence);

		if (smps[j]->flags & SAMPLE_HAS_ORIGIN) {
			uint64_t refrtm = smps[j]->ts.origin.tv_sec * 1000 + smps[j]->ts.origin.tv_nsec / 1000000;

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
		.init		= iec61850_init,
		.deinit		= iec61850_deinit,
		.parse		= iec61850_sv_parse,
		.print		= iec61850_sv_print,
		.start		= iec61850_sv_start,
		.stop		= iec61850_sv_stop,
		.destroy	= iec61850_sv_destroy,
		.read		= iec61850_sv_read,
		.write		= iec61850_sv_write,
		.fd		= iec61850_sv_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)

#endif /* CONFIG_IEC61850_SAMPLED_VALUES_SUPPORT */
