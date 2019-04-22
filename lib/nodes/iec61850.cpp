/** Node type: IEC 61850-9-2 (Sampled Values)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <villas/nodes/iec61850_sv.h>
#include <villas/plugin.h>
#include <villas/signal.h>

#define CONFIG_SV_DEFAULT_APPID 0x4000
#define CONFIG_SV_DEFAULT_DST_ADDRESS CONFIG_GOOSE_DEFAULT_DST_ADDRESS
#define CONFIG_SV_DEFAULT_PRIORITY 4
#define CONFIG_SV_DEFAULT_VLAN_ID 0

const struct iec61850_type_descriptor type_descriptors[] = {
	/* name,              iec_type,                         type,                 size, supported */
	{ "boolean",          iec61850_type::BOOLEAN,		SIGNAL_TYPE_BOOLEAN,	 1, false, false },
	{ "int8",             iec61850_type::INT8,		SIGNAL_TYPE_INTEGER,	 1, false, false },
	{ "int16",            iec61850_type::INT16,		SIGNAL_TYPE_INTEGER,	 2, false, false },
	{ "int32",            iec61850_type::INT32,		SIGNAL_TYPE_INTEGER,	 4, false, false },
	{ "int64",            iec61850_type::INT64,		SIGNAL_TYPE_INTEGER,	 8, false, false },
	{ "int8u",            iec61850_type::INT8U,		SIGNAL_TYPE_INTEGER,	 1, false, false },
	{ "int16u",           iec61850_type::INT16U,		SIGNAL_TYPE_INTEGER,	 2, false, false },
	{ "int32u",           iec61850_type::INT32U,		SIGNAL_TYPE_INTEGER,	 4, false, false },
	{ "int64u",           iec61850_type::INT64U,		SIGNAL_TYPE_INTEGER,	 8, false, false },
	{ "float32",          iec61850_type::FLOAT32,		SIGNAL_TYPE_FLOAT,	 4, false, false },
	{ "float64",          iec61850_type::FLOAT64,		SIGNAL_TYPE_FLOAT,	 8, false, false },
	{ "enumerated",       iec61850_type::ENUMERATED,		SIGNAL_TYPE_INVALID,	 4, false, false },
	{ "coded_enum",       iec61850_type::CODED_ENUM,		SIGNAL_TYPE_INVALID,	 4, false, false },
	{ "octet_string",     iec61850_type::OCTET_STRING,	SIGNAL_TYPE_INVALID,	20, false, false },
	{ "visible_string",   iec61850_type::VISIBLE_STRING,	SIGNAL_TYPE_INVALID,	35, false, false },
	{ "objectname",       iec61850_type::OBJECTNAME,		SIGNAL_TYPE_INVALID,	20, false, false },
	{ "objectreference",  iec61850_type::OBJECTREFERENCE,	SIGNAL_TYPE_INVALID,	20, false, false },
	{ "timestamp",        iec61850_type::TIMESTAMP,		SIGNAL_TYPE_INVALID,	 8, false, false },
	{ "entrytime",        iec61850_type::ENTRYTIME,		SIGNAL_TYPE_INVALID,	 6, false, false },
	{ "bitstring",        iec61850_type::BITSTRING,		SIGNAL_TYPE_INVALID,	 4, false, false }
};

/** Each network interface needs a separate receiver */
static struct vlist receivers;
static pthread_t thread;
static EthernetHandleSet hset;
static int users = 0;

static void * iec61850_thread(void *ctx)
{
	int ret;

	while (1) {
		ret = EthernetHandleSet_waitReady(hset, 1000);
		if (ret < 0)
			continue;

		for (unsigned i = 0; i < vlist_length(&receivers); i++) {
			struct iec61850_receiver *r = (struct iec61850_receiver *) vlist_at(&receivers, i);

			switch (r->type) {
				case iec61850_receiver::type::GOOSE:	GooseReceiver_tick(r->goose); break;
				case iec61850_receiver::type::SAMPLED_VALUES:	SVReceiver_tick(r->sv); break;
			}
		}
	}

	return nullptr;
}

const struct iec61850_type_descriptor * iec61850_lookup_type(const char *name)
{
	for (unsigned i = 0; i < ARRAY_LEN(type_descriptors); i++) {
		if (!strcmp(name, type_descriptors[i].name))
			return &type_descriptors[i];
	}

	return nullptr;
}

int iec61850_parse_signals(json_t *json_signals, struct vlist *signals, struct vlist *node_signals)
{
	int ret, total_size = 0;
	const char *iec_type;
	const struct iec61850_type_descriptor *td;
	struct signal *sig;
	json_error_t err;

	ret = vlist_init(signals);
	if (ret)
		return ret;

	if (json_is_array(json_signals)) {
		json_t *json_signal;
		size_t i;
		json_array_foreach(json_signals, i, json_signal) {
			json_unpack_ex(json_signal, &err, 0, "{ s?: s }",
				"iec_type", &iec_type
			);

			/* Try to deduct the IEC 61850 data type from VILLAS signal format */
			if (!iec_type) {
				if (!node_signals)
					return -1;

				sig = (struct signal *) vlist_at(node_signals, i);
				if (!sig)
					return -1;

				switch (sig->type) {
					case SIGNAL_TYPE_BOOLEAN:
						iec_type = "boolean";
						break;

					case SIGNAL_TYPE_FLOAT:
						iec_type = "float64";
						break;

					case SIGNAL_TYPE_INTEGER:
						iec_type = "int64";
						break;

					default:
						return -1;
				}
			}

			td = iec61850_lookup_type(iec_type);
			if (!td)
				return -1;

			vlist_push(signals, (void *) td);

			total_size += td->size;
		}
	}
	else {
		ret = json_unpack_ex(json_signals, &err, 0, "{ s: s }", "iec_type", &iec_type);
		if (ret)
			return ret;

		td = iec61850_lookup_type(iec_type);
		if (!td)
			return -1;

		for (unsigned i = 0; i < vlist_length(node_signals); i++) {
			vlist_push(signals, (void *) td);

			total_size += td->size;
		}
	}

	return total_size;
}

int iec61850_type_start(struct super_node *sn)
{
	int ret;

	/* Check if already initialized */
	if (users > 0)
		return 0;

	ret = vlist_init(&receivers);
	if (ret)
		return ret;

	hset = EthernetHandleSet_new();

	ret = pthread_create(&thread, nullptr, iec61850_thread, nullptr);
	if (ret)
		return ret;

	return 0;
}

int iec61850_type_stop()
{
	int ret;

	if (--users > 0)
		return 0;

	for (unsigned i = 0; i < vlist_length(&receivers); i++) {
		struct iec61850_receiver *r = (struct iec61850_receiver *) vlist_at(&receivers, i);

		iec61850_receiver_stop(r);
	}

	ret = pthread_cancel(thread);
	if (ret)
		return ret;

	ret = pthread_join(thread, nullptr);
	if (ret)
		return ret;

	EthernetHandleSet_destroy(hset);

	vlist_destroy(&receivers, (dtor_cb_t) iec61850_receiver_destroy, true);

	return 0;
}

int iec61850_receiver_start(struct iec61850_receiver *r)
{
	switch (r->type) {
		case iec61850_receiver::type::GOOSE:
			r->socket = GooseReceiver_startThreadless(r->goose);
			break;

		case iec61850_receiver::type::SAMPLED_VALUES:
			r->socket = SVReceiver_startThreadless(r->sv);
			break;
	}

	EthernetHandleSet_addSocket(hset, r->socket);

	return 0;
}

int iec61850_receiver_stop(struct iec61850_receiver *r)
{
	EthernetHandleSet_removeSocket(hset, r->socket);

	switch (r->type) {
		case iec61850_receiver::type::GOOSE:
			GooseReceiver_stopThreadless(r->goose);
			break;

		case iec61850_receiver::type::SAMPLED_VALUES:
			SVReceiver_stopThreadless(r->sv);
			break;
	}

	return 0;
}

int iec61850_receiver_destroy(struct iec61850_receiver *r)
{
	switch (r->type) {
		case iec61850_receiver::type::GOOSE:
			GooseReceiver_destroy(r->goose);
			break;

		case iec61850_receiver::type::SAMPLED_VALUES:
			SVReceiver_destroy(r->sv);
			break;
	}

	free(r->interface);

	return 0;
}

struct iec61850_receiver * iec61850_receiver_lookup(enum iec61850_receiver::type t, const char *intf)
{
	for (unsigned i = 0; i < vlist_length(&receivers); i++) {
		struct iec61850_receiver *r = (struct iec61850_receiver *) vlist_at(&receivers, i);

		if (r->type == t && strcmp(r->interface, intf) == 0)
			return r;
	}

	return nullptr;
}

struct iec61850_receiver * iec61850_receiver_create(enum iec61850_receiver::type t, const char *intf)
{
	struct iec61850_receiver *r;

	/* Check if there is already a SVReceiver for this interface */
	r = iec61850_receiver_lookup(t, intf);
	if (!r) {
		r = (struct iec61850_receiver *) alloc(sizeof(struct iec61850_receiver));
		if (!r)
			return nullptr;

		r->interface = strdup(intf);
		r->type = t;

		switch (r->type) {
			case iec61850_receiver::type::GOOSE:
				r->goose = GooseReceiver_create();
				GooseReceiver_setInterfaceId(r->goose, r->interface);
				break;

			case iec61850_receiver::type::SAMPLED_VALUES:
				r->sv = SVReceiver_create();
				SVReceiver_setInterfaceId(r->sv, r->interface);
				break;
		}

		iec61850_receiver_start(r);

		vlist_push(&receivers, r);
	}

	return r;
}
