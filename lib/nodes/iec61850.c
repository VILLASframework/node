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

#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <villas/nodes/iec61850_sv.h>
#include <villas/plugin.h>

#define CONFIG_SV_DEFAULT_APPID 0x4000
#define CONFIG_SV_DEFAULT_DST_ADDRESS CONFIG_GOOSE_DEFAULT_DST_ADDRESS
#define CONFIG_SV_DEFAULT_PRIORITY 4
#define CONFIG_SV_DEFAULT_VLAN_ID 0

const struct iec61850_type_descriptor type_descriptors[] = {
	/* name,              fmt, type,                        size, supported */
	{ "boolean",          'b', IEC61850_TYPE_BOOLEAN,          1, false, false },
	{ "int8",             'o', IEC61850_TYPE_INT8,             1, false, false },
	{ "int16",            'w', IEC61850_TYPE_INT16,            2, false, false },
	{ "int32",            'd', IEC61850_TYPE_INT32,            4, false, false },
	{ "int64",            'g', IEC61850_TYPE_INT64,            8, false, false },
	{ "int8u",            'O', IEC61850_TYPE_INT8U,            1, false, false },
	{ "int16u",           'W', IEC61850_TYPE_INT16U,           2, false, false },
	{ "int32u",           'D', IEC61850_TYPE_INT32U,           4, false, false },
	{ "int64u",           'G', IEC61850_TYPE_INT64U,           8, false, false },
	{ "float32",          'f', IEC61850_TYPE_FLOAT32,          4, false, false },
	{ "float64",          'F', IEC61850_TYPE_FLOAT64,          8, false, false },
	{ "enumerated",       'e', IEC61850_TYPE_ENUMERATED,       4, false, false },
	{ "coded_enum",       'c', IEC61850_TYPE_CODED_ENUM,       4, false, false },
	{ "octet_string",     's', IEC61850_TYPE_OCTET_STRING,    20, false, false },
	{ "visible_string",   'S', IEC61850_TYPE_VISIBLE_STRING,  35, false, false },
	{ "objectname",       'n', IEC61850_TYPE_OBJECTNAME,      20, false, false },
	{ "objectreference",  'r', IEC61850_TYPE_OBJECTREFERENCE, 20, false, false },
	{ "timestamp",        't', IEC61850_TYPE_TIMESTAMP,        8, false, false },
	{ "entrytime",        'e', IEC61850_TYPE_ENTRYTIME,        6, false, false },
	{ "bitstring",        'B', IEC61850_TYPE_BITSTRING,        4, false, false }
};

/** Each network interface needs a separate receiver */
static struct list receivers = LIST_INIT();
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

const struct iec61850_type_descriptor * iec61850_lookup_type(const char *name, char fmt)
{
	/* Either name or fmt argument must be given */
	if ((fmt && name) || (!fmt && !name))
		return NULL;

	for (unsigned i = 0; i < ARRAY_LEN(type_descriptors); i++) {
		if ((name && !strcmp(name, type_descriptors[i].name)) ||
		    (fmt  && fmt == type_descriptors[i].format))
			return &type_descriptors[i];
	}

	return NULL;
}

int iec61850_parse_mapping(json_t *json_mapping, struct list *mapping)
{
	int total_size = 0;

	list_init(mapping);

	if (json_is_array(json_mapping)) {
		json_t *json_field;
		size_t index;

		json_array_foreach(json_mapping, index, json_field) {
			const struct iec61850_type_descriptor *m;
			const char *type = json_string_value(json_field);

			if (!json_is_string(json_field))
				return -1;

			m = iec61850_lookup_type(type, 0);
			if (!m)
				return -1;

			list_push(mapping, (void *) m);

			total_size += m->size;
		}
	}
	else if (json_is_string(json_mapping)) {
		const struct iec61850_type_descriptor *m;
		const char *format_str = json_string_value(json_mapping);

		for (int i = 0; format_str[i]; i++) {
			m = iec61850_lookup_type(NULL, format_str[i]);
			if (!m)
				return -1;

			list_push(mapping, (void *) m);

			total_size += m->size;
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
		case IEC61850_RECEIVER_GOOSE:	r->socket = GooseReceiver_startThreadless(r->goose); break;
		case IEC61850_RECEIVER_SV:	r->socket = SVReceiver_startThreadless(r->sv); break;
	}

	EthernetHandleSet_addSocket(hset, r->socket);

	return 0;
}

int iec61850_receiver_stop(struct iec61850_receiver *r)
{
	EthernetHandleSet_removeSocket(hset, r->socket);

	switch (r->type) {
		case IEC61850_RECEIVER_GOOSE:	GooseReceiver_stopThreadless(r->goose); break;
		case IEC61850_RECEIVER_SV:	SVReceiver_stopThreadless(r->sv); break;
	}

	return 0;
}

int iec61850_receiver_destroy(struct iec61850_receiver *r)
{
	switch (r->type) {
		case IEC61850_RECEIVER_GOOSE:	GooseReceiver_destroy(r->goose); break;
		case IEC61850_RECEIVER_SV:	SVReceiver_destroy(r->sv); break;
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
