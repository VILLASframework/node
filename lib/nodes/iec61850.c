/** Node type: IEC 61850-9-2 (Sampled Values)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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
	/* name,              type,                             format,               size, supported */
	{ "boolean",          IEC61850_TYPE_BOOLEAN,		SIGNAL_TYPE_BOOLEAN,	 1, false, false },
	{ "int8",             IEC61850_TYPE_INT8,		SIGNAL_TYPE_INTEGER,	 1, false, false },
	{ "int16",            IEC61850_TYPE_INT16,		SIGNAL_TYPE_INTEGER,	 2, false, false },
	{ "int32",            IEC61850_TYPE_INT32,		SIGNAL_TYPE_INTEGER,	 4, false, false },
	{ "int64",            IEC61850_TYPE_INT64,		SIGNAL_TYPE_INTEGER,	 8, false, false },
	{ "int8u",            IEC61850_TYPE_INT8U,		SIGNAL_TYPE_INTEGER,	 1, false, false },
	{ "int16u",           IEC61850_TYPE_INT16U,		SIGNAL_TYPE_INTEGER,	 2, false, false },
	{ "int32u",           IEC61850_TYPE_INT32U,		SIGNAL_TYPE_INTEGER,	 4, false, false },
	{ "int64u",           IEC61850_TYPE_INT64U,		SIGNAL_TYPE_INTEGER,	 8, false, false },
	{ "float32",          IEC61850_TYPE_FLOAT32,		SIGNAL_TYPE_FLOAT,	 4, false, false },
	{ "float64",          IEC61850_TYPE_FLOAT64,		SIGNAL_TYPE_FLOAT,	 8, false, false },
	{ "enumerated",       IEC61850_TYPE_ENUMERATED,		SIGNAL_TYPE_INVALID,	 4, false, false },
	{ "coded_enum",       IEC61850_TYPE_CODED_ENUM,		SIGNAL_TYPE_INVALID,	 4, false, false },
	{ "octet_string",     IEC61850_TYPE_OCTET_STRING,	SIGNAL_TYPE_INVALID,	20, false, false },
	{ "visible_string",   IEC61850_TYPE_VISIBLE_STRING,	SIGNAL_TYPE_INVALID,	35, false, false },
	{ "objectname",       IEC61850_TYPE_OBJECTNAME,		SIGNAL_TYPE_INVALID,	20, false, false },
	{ "objectreference",  IEC61850_TYPE_OBJECTREFERENCE,	SIGNAL_TYPE_INVALID,	20, false, false },
	{ "timestamp",        IEC61850_TYPE_TIMESTAMP,		SIGNAL_TYPE_INVALID,	 8, false, false },
	{ "entrytime",        IEC61850_TYPE_ENTRYTIME,		SIGNAL_TYPE_INVALID,	 6, false, false },
	{ "bitstring",        IEC61850_TYPE_BITSTRING,		SIGNAL_TYPE_INVALID,	 4, false, false }
};

/** Each network interface needs a separate receiver */
static struct list receivers;
static pthread_t thread;
static EthernetHandleSet hset;
static int users = 0;

LIST_INIT_STATIC(&receivers);

static void * iec61850_thread(void *ctx)
{
	int ret;

	while (1) {
		ret = EthernetHandleSet_waitReady(hset, 1000);
		if (ret < 0)
			continue;

		for (unsigned i = 0; i < list_length(&receivers); i++) {
			struct iec61850_receiver *r = (struct iec61850_receiver *) list_at(&receivers, i);

			switch (r->type) {
				case IEC61850_RECEIVER_GOOSE:	GooseReceiver_tick(r->goose); break;
				case IEC61850_RECEIVER_SV:	SVReceiver_tick(r->sv); break;
			}
		}
	}

	return NULL;
}

const struct iec61850_type_descriptor * iec61850_lookup_type(const char *name)
{
	for (unsigned i = 0; i < ARRAY_LEN(type_descriptors); i++) {
		if (!strcmp(name, type_descriptors[i].name))
			return &type_descriptors[i];
	}

	return NULL;
}

int iec61850_parse_signals(json_t *json_signals, struct list *signals, struct list *node_signals)
{
	int ret, total_size = 0;
	const char *iec_type;

	ret = list_init(signals);
	if (ret)
		return ret;

	json_t *json_signal;
	size_t i;
	json_array_foreach(json_signals, i, json_signal) {
		const struct iec61850_type_descriptor *td;
		struct signal *sig;

		json_unpack(json_signal, "{ s?: s }",
			"iec_type", &iec_type
		);

		/* Try to deduct the IEC 61850 data type from VILLAS signal format */
		if (!iec_type) {
			if (!node_signals)
				return -1;

			sig = list_at(node_signals, i);
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

		list_push(signals, (void *) td);

		total_size += td->size;
	}

	return total_size;
}

int iec61850_type_start()
{
	int ret;

	/* Check if already initialized */
	if (users > 0)
		return 0;

	hset = EthernetHandleSet_new();

	ret = pthread_create(&thread, NULL, iec61850_thread, NULL);
	if (ret)
		return ret;

	return 0;
}

int iec61850_type_stop()
{
	int ret;

	if (--users > 0)
		return 0;

	for (unsigned i = 0; i < list_length(&receivers); i++) {
		struct iec61850_receiver *r = (struct iec61850_receiver *) list_at(&receivers, i);

		iec61850_receiver_stop(r);
	}

	ret = pthread_cancel(thread);
	if (ret)
		return ret;

	ret = pthread_join(thread, NULL);
	if (ret)
		return ret;

	EthernetHandleSet_destroy(hset);

	list_destroy(&receivers, (dtor_cb_t) iec61850_receiver_destroy, true);

	return 0;
}

int iec61850_receiver_start(struct iec61850_receiver *r)
{
	switch (r->type) {
		case IEC61850_RECEIVER_GOOSE:
			r->socket = GooseReceiver_startThreadless(r->goose);
			break;

		case IEC61850_RECEIVER_SV:
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
		case IEC61850_RECEIVER_GOOSE:
			GooseReceiver_stopThreadless(r->goose);
			break;

		case IEC61850_RECEIVER_SV:
			SVReceiver_stopThreadless(r->sv);
			break;
	}

	return 0;
}

int iec61850_receiver_destroy(struct iec61850_receiver *r)
{
	switch (r->type) {
		case IEC61850_RECEIVER_GOOSE:
			GooseReceiver_destroy(r->goose);
			break;

		case IEC61850_RECEIVER_SV:
			SVReceiver_destroy(r->sv);
			break;
	}

	free(r->interface);

	return 0;
}

struct iec61850_receiver * iec61850_receiver_lookup(enum iec61850_receiver_type t, const char *intf)
{
	for (unsigned i = 0; i < list_length(&receivers); i++) {
		struct iec61850_receiver *r = (struct iec61850_receiver *) list_at(&receivers, i);

		if (r->type == t && strcmp(r->interface, intf) == 0)
			return r;
	}

	return NULL;
}

struct iec61850_receiver * iec61850_receiver_create(enum iec61850_receiver_type t, const char *intf)
{
	struct iec61850_receiver *r;

	/* Check if there is already a SVReceiver for this interface */
	r = iec61850_receiver_lookup(t, intf);
	if (!r) {
		r = alloc(sizeof(struct iec61850_receiver));
		if (!r)
			return NULL;

		r->interface = strdup(intf);
		r->type = t;

		switch (r->type) {
			case IEC61850_RECEIVER_GOOSE:
				r->goose = GooseReceiver_create();
				GooseReceiver_setInterfaceId(r->goose, r->interface);
				break;

			case IEC61850_RECEIVER_SV:
				r->sv = SVReceiver_create();
				SVReceiver_setInterfaceId(r->sv, r->interface);
				break;
		}

		iec61850_receiver_start(r);

		list_push(&receivers, r);
	}

	return r;
}
