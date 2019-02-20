/** Node-type for uldaq connections.
 *
 * @file
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
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

#include <string.h>

#include <villas/node.h>
#include <villas/plugin.h>
#include <villas/config.h>
#include <villas/nodes/uldaq.h>
#include <villas/memory.h>

static unsigned num_devs = ULDAQ_MAX_DEV_COUNT;
static DaqDeviceDescriptor descriptors[ULDAQ_MAX_DEV_COUNT];

static const struct {
	const char *name;
	AiInputMode mode;
} input_modes[] = {
	{ "differential", AI_DIFFERENTIAL },
	{ "single-ended", AI_SINGLE_ENDED },
	{ "pseudo-differential", AI_PSEUDO_DIFFERENTIAL }
};

static const struct {
	const char *name;
	DaqDeviceInterface interface;
} interface_types[] = {
	{ "usb", USB_IFC },
	{ "bluetooth", BLUETOOTH_IFC },
	{ "ethernet", ETHERNET_IFC },
	{ "any", ANY_IFC }
};

static const struct {
	const char *name;
	Range range;
	float min, max;
} ranges[] = {
	{ "bipolar-60", BIP60VOLTS,      -60.0,  +60.0   },
	{ "bipolar-60", BIP60VOLTS,      -60.0,  +60.0   },
	{ "bipolar-30", BIP30VOLTS,      -30.0,  +30.0   },
	{ "bipolar-15", BIP15VOLTS,      -15.0,  +15.0   },
	{ "bipolar-20", BIP20VOLTS,      -20.0,  +20.0   },
	{ "bipolar-10", BIP10VOLTS,      -10.0,  +10.0   },
	{ "bipolar-5", BIP5VOLTS,         -5.0,   +5.0   },
	{ "bipolar-4", BIP4VOLTS,         -4.0,   +4.0   },
	{ "bipolar-2.5", BIP2PT5VOLTS,    -2.5,   +2.5   },
	{ "bipolar-2", BIP2VOLTS,         -2.0,   +2.0   },
	{ "bipolar-1.25", BIP1PT25VOLTS,  -1.25,  +1.25  },
	{ "bipolar-1", BIP1VOLTS,         -1.0,   +1.0   },
	{ "bipolar-0.625", BIPPT625VOLTS, -0.625, +0.625 },
	{ "bipolar-0.5", BIPPT5VOLTS,     -0.5,   +0.5   },
	{ "bipolar-0.25", BIPPT25VOLTS,   -0.25,  +0.25  },
	{ "bipolar-0.125", BIPPT125VOLTS, -0.125, +0.125 },
	{ "bipolar-0.2", BIPPT2VOLTS,     -0.2,   +0.2   },
	{ "bipolar-0.1", BIPPT1VOLTS,     -0.1,   +0.1   },
	{ "bipolar-0.078", BIPPT078VOLTS, -0.078, +0.078 },
	{ "bipolar-0.05", BIPPT05VOLTS,   -0.05,  +0.05  },
	{ "bipolar-0.01", BIPPT01VOLTS,   -0.01,  +0.01  },
	{ "bipolar-0.005", BIPPT005VOLTS, -0.005, +0.005 },
	{ "unipolar-60", UNI60VOLTS ,      0.0,  +60.0   },
	{ "unipolar-30", UNI30VOLTS ,      0.0,  +30.0   },
	{ "unipolar-15", UNI15VOLTS ,      0.0,  +15.0   },
	{ "unipolar-20", UNI20VOLTS ,      0.0,  +20.0   },
	{ "unipolar-10", UNI10VOLTS ,      0.0,  +10.0   },
	{ "unipolar-5", UNI5VOLTS ,        0.0,   +5.0   },
	{ "unipolar-4", UNI4VOLTS ,        0.0,   +4.0   },
	{ "unipolar-2.5", UNI2PT5VOLTS,    0.0,   +2.5   },
	{ "unipolar-2", UNI2VOLTS ,        0.0,   +2.0   },
	{ "unipolar-1.25", UNI1PT25VOLTS,  0.0,   +1.25  },
	{ "unipolar-1", UNI1VOLTS ,        0.0,   +1.0   },
	{ "unipolar-0.625", UNIPT625VOLTS, 0.0,   +0.625 },
	{ "unipolar-0.5", UNIPT5VOLTS,     0.0,   +0.5   },
	{ "unipolar-0.25", UNIPT25VOLTS,   0.0,   +0.25  },
	{ "unipolar-0.125", UNIPT125VOLTS, 0.0,   +0.125 },
	{ "unipolar-0.2", UNIPT2VOLTS,     0.0,   +0.2   },
	{ "unipolar-0.1", UNIPT1VOLTS,     0.0,   +0.1   },
	{ "unipolar-0.078", UNIPT078VOLTS, 0.0,   +0.078 },
	{ "unipolar-0.05", UNIPT05VOLTS,   0.0,   +0.05  },
	{ "unipolar-0.01", UNIPT01VOLTS,   0.0,   +0.01  },
	{ "unipolar-0.005", UNIPT005VOLTS, 0.0,   +0.005 }
};

static AiInputMode uldaq_parse_input_mode(const char *str)
{
	for (int i = 0; i < ARRAY_LEN(input_modes); i++) {
		if (!strcmp(input_modes[i].name, str))
			return input_modes[i].mode;
	}

	return -1;
}

static DaqDeviceInterface uldaq_parse_interface_type(const char *str)
{
	for (int i = 0; i < ARRAY_LEN(interface_types); i++) {
		if (!strcmp(interface_types[i].name, str))
			return interface_types[i].interface;
	}

	return -1;
}

static const char * uldaq_print_interface_type(DaqDeviceInterface iftype)
{
	for (int i = 0; i < ARRAY_LEN(interface_types); i++) {
		if (interface_types[i].interface == iftype)
			return interface_types[i].name;
	}

	return NULL;
}

static Range uldaq_parse_range(const char *str)
{
	for (int i = 0; i < ARRAY_LEN(ranges); i++) {
		if (!strcmp(ranges[i].name, str))
			return ranges[i].range;
	}

	return -1;
}

static DaqDeviceDescriptor * uldaq_find_device(struct uldaq *u) {
	DaqDeviceDescriptor *d = NULL;

	if (num_devs == 0)
		return NULL;

	if (u->device_interface_type == ANY_IFC && u->device_id == NULL)
		return &descriptors[0];

	for (int i = 0; i < num_devs; i++) {
		d = &descriptors[i];

		if (u->device_id) {
			if (strcmp(u->device_id, d->uniqueId))
				break;
		}

		if (u->device_interface_type != ANY_IFC) {
			if (u->device_interface_type != d->devInterface)
				break;
		}

		return d;
	}

	return NULL;
}

static int uldaq_connect(struct node *n)
{
	struct uldaq *u = (struct uldaq *) n->_vd;
	UlError err;

	/* Find Matching device */
	if (!u->device_descriptor) {
		u->device_descriptor = uldaq_find_device(u);
		if (!u->device_descriptor) {
			warning("Unable to find a matching device for node '%s'", node_name(n));
			return -1;
		}
	}

	/* Get a handle to the DAQ device associated with the first descriptor */
	if (!u->device_handle) {
		u->device_handle = ulCreateDaqDevice(descriptors[0]);
		if (!u->device_handle) {
			warning("Unable to create handle for DAQ device for node '%s'", node_name(n));
			return -1;
		}
	}

	/* Check if device is already connected */
	int connected;
	err = ulIsDaqDeviceConnected(u->device_handle, &connected);
	if (err != ERR_NO_ERROR)
		return -1;

	/* Connect to device */
	if (!connected) {
		err = ulConnectDaqDevice(u->device_handle);
		if (err != ERR_NO_ERROR) {
			warning("Failed to connect to DAQ device for node '%s'", node_name(n));
			return -1;
		}
	}

	return 0;
}

int uldaq_type_start(struct super_node *sn)
{
	UlError err;

	/* Get descriptors for all of the available DAQ devices */
	err = ulGetDaqDeviceInventory(ANY_IFC, descriptors, &num_devs);
	if (err != ERR_NO_ERROR) {
		warning("Failed to retrieve DAQ device list");
		return -1;
	}

	info("Found %d DAQ devices", num_devs);
	for (int i = 0; i < num_devs; i++) {
		DaqDeviceDescriptor *desc = &descriptors[i];

		info("  %d: %s %s (%s)", i, desc->uniqueId, desc->devString,  uldaq_print_interface_type(desc->devInterface));
	}

	return 0;
}

int uldaq_init(struct node *n)
{
	int ret;
	struct uldaq *u = (struct uldaq *) n->_vd;

	u->device_id = NULL;
	u->device_interface_type = ANY_IFC;

	u->in.queues = NULL;
	u->in.sample_rate = 1000;
	u->in.scan_options = (ScanOption) (SO_DEFAULTIO | SO_CONTINUOUS);
	u->in.flags = AINSCAN_FF_DEFAULT;

	ret = pthread_mutex_init(&u->in.mutex, NULL);
	if (ret)
		return ret;

	ret = pthread_cond_init(&u->in.cv, NULL);
	if (ret)
		return ret;

	return 0;
}

int uldaq_destroy(struct node *n)
{
	int ret;
	struct uldaq *u = (struct uldaq *) n->_vd;

	if (u->in.queues)
		free(u->in.queues);

	ret = pthread_mutex_destroy(&u->in.mutex);
	if (ret)
		return ret;

	ret = pthread_cond_destroy(&u->in.cv);
	if (ret)
		return ret;

	return 0;
}

int uldaq_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct uldaq *u = (struct uldaq *) n->_vd;

	const char *default_range_str = NULL;
	const char *default_input_mode_str = NULL;
	const char *interface_type = NULL;

	size_t i;
	json_t *json_signals;
	json_t *json_signal;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: s, s: { s: o, s: F, s?: s, s?: s } }",
		"interface_type", &interface_type,
		"device_id", &u->device_id,
		"in",
			"signals", &json_signals,
			"sample_rate", &u->in.sample_rate,
			"range", &default_range_str,
			"input_mode", &default_input_mode_str
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (interface_type) {
		int iftype = uldaq_parse_interface_type(interface_type);
		if (iftype < 0)
			error("Invalid interface type: %s for node '%s'", interface_type, node_name(n));

		u->device_interface_type = iftype;
	}

	u->in.channel_count = vlist_length(&n->in.signals);
	u->in.queues = realloc(u->in.queues, sizeof(struct AiQueueElement) * u->in.channel_count);

	json_array_foreach(json_signals, i, json_signal) {
		const char *range_str = NULL, *input_mode_str = NULL;
		int channel = -1, input_mode, range;

		ret = json_unpack_ex(json_signal, &err, 0, "{ s?: s, s?: s, s?: i }",
			"range", &range_str,
			"input_mode", &input_mode_str,
			"channel", &channel
		);
		if (ret)
			jerror(&err, "Failed to parse signal configuration of node %s", node_name(n));

		if (!range_str)
			range_str = default_range_str;

		if (!input_mode_str)
			input_mode_str = default_input_mode_str;

		if (channel < 0)
			channel = i;

		if (!range_str)
			error("No input range specified for signal %zu of node %s.", i, node_name(n));

		if (!input_mode_str)
			error("No input mode specified for signal %zu of node %s.", i, node_name(n));

		range = uldaq_parse_range(range_str);
		if (range < 0)
			error("Invalid input range specified for signal %zu of node %s.", i, node_name(n));

		input_mode = uldaq_parse_input_mode(input_mode_str);
		if (input_mode < 0)
			error("Invalid input mode specified for signal %zu of node %s.", i, node_name(n));

		u->in.queues[i].range = range;
		u->in.queues[i].inputMode = input_mode;
		u->in.queues[i].channel = channel;
	}

	return ret;
}

char * uldaq_print(struct node *n)
{
	struct uldaq *u = (struct uldaq *) n->_vd;

	char *buf = NULL;

	if (u->device_descriptor) {
		char *uid = u->device_descriptor->uniqueId;
		char *name = u->device_descriptor->productName;
		const char *iftype = uldaq_print_interface_type(u->device_descriptor->devInterface);

		buf = strcatf(&buf, "device=%s (%s), interface=%s", uid, name, iftype);
	}
	else {
		const char *uid = u->device_id;
		const char *iftype = uldaq_print_interface_type(u->device_interface_type);

		buf = strcatf(&buf, "device=%s, interface=%s", uid, iftype);
	}

	buf = strcatf(&buf, ", in.sample_rate=%f", u->in.sample_rate);

	return buf;
}

int uldaq_check(struct node *n)
{
	int ret;
	long long has_ai, event_types, max_channel, scan_options, num_ranges_se, num_ranges_diff;
	struct uldaq *u = (struct uldaq *) n->_vd;

	UlError err;

	if (n->in.vectorize < 100) {
		warning("vectorize setting of node '%s' must be larger than 100", node_name(n));
		return -1;
	}

	ret = uldaq_connect(n);
	if (ret)
		return ret;

	err = ulDevGetInfo(u->device_handle, DEV_INFO_HAS_AI_DEV, 0, &has_ai);
	if (err != ERR_NO_ERROR)
		return -1;

	err = ulDevGetInfo(u->device_handle, DEV_INFO_DAQ_EVENT_TYPES, 0, &event_types);
	if (err != ERR_NO_ERROR)
		return -1;

	err = ulAIGetInfo(u->device_handle, AI_INFO_NUM_CHANS, 0, &max_channel);
	if (err != ERR_NO_ERROR)
		return -1;

	err = ulAIGetInfo(u->device_handle, AI_INFO_SCAN_OPTIONS, 0, &scan_options);
	if (err != ERR_NO_ERROR)
		return -1;

	err = ulAIGetInfo(u->device_handle, AI_INFO_NUM_DIFF_RANGES, 0, &num_ranges_diff);
	if (err != ERR_NO_ERROR)
		return -1;

	err = ulAIGetInfo(u->device_handle, AI_INFO_NUM_SE_RANGES, 0, &num_ranges_se);
	if (err != ERR_NO_ERROR)
		return -1;

	Range ranges_diff[num_ranges_diff];
	Range ranges_se[num_ranges_se];

	for (int i = 0; i < num_ranges_diff; i++) {
		err = ulAIGetInfo(u->device_handle, AI_INFO_DIFF_RANGE, i, (long long *) &ranges_diff[i]);
		if (err != ERR_NO_ERROR)
			return -1;
	}

	for (int i = 0; i < num_ranges_se; i++) {
		err = ulAIGetInfo(u->device_handle, AI_INFO_SE_RANGE, i, (long long *) &ranges_se[i]);
		if (err != ERR_NO_ERROR)
			return -1;
	}

	if (!has_ai) {
		warning("DAQ device has no analog input channels");
		return -1;
	}

	if (!(event_types & DE_ON_DATA_AVAILABLE)) {
		warning("DAQ device does not support events");
		return -1;
	}

	if ((scan_options & u->in.scan_options) != u->in.scan_options) {
		warning("DAQ device does not support required scan options");
		return -1;
	}

	for (size_t i = 0; i < vlist_length(&n->in.signals); i++) {
		struct signal *s = (struct signal *) vlist_at(&n->in.signals, i);
		AiQueueElement *q = &u->in.queues[i];

		if (s->type != SIGNAL_TYPE_FLOAT) {
			warning("Node '%s' only supports signals of type = float!", node_name(n));
			return -1;
		}

		switch (q->inputMode) {
			case AI_PSEUDO_DIFFERENTIAL:
			case AI_DIFFERENTIAL:
				for (int j = 0; j < num_ranges_diff; j++) {
					if (q->range == ranges_diff[j])
						goto found;
				}
				break;

			case AI_SINGLE_ENDED:
				for (int j = 0; j < num_ranges_se; j++) {
					if (q->range == ranges_se[j])
						goto found;
				}
				break;
		}

		warning("Unsupported range for signal %zu", i);
		return -1;

found:		if (q->channel > max_channel) {
			warning("DAQ device does not support more than %lld channels", max_channel);
			return -1;
		}
	}

	return 0;
}

void uldaq_data_available(DaqDeviceHandle device_handle, DaqEventType event_type, unsigned long long event_data, void *ctx)
{
	struct node *n = (struct node *) ctx;
	struct uldaq *u = (struct uldaq *) n->_vd;

	pthread_mutex_lock(&u->in.mutex);

	UlError err;
	err = ulAInScanStatus(device_handle, &u->in.status, &u->in.transfer_status);
	if (err != ERR_NO_ERROR)
		warning("Failed to retrieve scan status in event callback");

	pthread_mutex_unlock(&u->in.mutex);

	/* Signal uldaq_read() about new data */
	pthread_cond_signal(&u->in.cv);
}

int uldaq_start(struct node *n)
{
	struct uldaq *u = (struct uldaq *) n->_vd;

	u->sequence = 0;
	u->in.buffer_pos = 0;

	int ret;
	UlError err;

	/* Allocate a buffer to receive the data */
	u->in.buffer_len = u->in.channel_count * n->in.vectorize * 50;
	u->in.buffer = (double *) alloc(u->in.buffer_len * sizeof(double));
	if (!u->in.buffer) {
		warning("Out of memory, unable to create scan buffer");
		return -1;
	}

	ret = uldaq_connect(n);
	if (ret)
		return ret;

	err = ulAInLoadQueue(u->device_handle, u->in.queues, vlist_length(&n->in.signals));
	if (err != ERR_NO_ERROR) {
		warning("Failed to load input queue to DAQ device for node '%s'", node_name(n));
		return -1;
	}

	/* Enable the event to be notified every time samples are available */
	err = ulEnableEvent(u->device_handle, DE_ON_DATA_AVAILABLE, n->in.vectorize, uldaq_data_available, n);

	/* Start the acquisition */
	err = ulAInScan(u->device_handle, 0, 0, 0, 0, u->in.buffer_len / u->in.channel_count, &u->in.sample_rate, u->in.scan_options, u->in.flags, u->in.buffer);
	if (err != ERR_NO_ERROR) {
		warning("Failed to start acquisition on DAQ device for node '%s'", node_name(n));
		return -1;
	}

	/* Get the initial status of the acquisition */
	err = ulAInScanStatus(u->device_handle, &u->in.status, &u->in.transfer_status);
	if (err != ERR_NO_ERROR) {
		warning("Failed to retrieve scan status on DAQ device for node '%s'", node_name(n));
		return -1;
	}

	if (u->in.status != SS_RUNNING) {
		warning("Acquisition did not start on DAQ device for node '%s'", node_name(n));
		return -1;
	}

	return 0;
}

int uldaq_stop(struct node *n)
{
	struct uldaq *u = (struct uldaq *) n->_vd;

	UlError err;

	// @todo: fix deadlock
	//pthread_mutex_lock(&u->in.mutex);

	/* Get the current status of the acquisition */
	err = ulAInScanStatus(u->device_handle, &u->in.status, &u->in.transfer_status);
	if (err != ERR_NO_ERROR)
		return -1;

	/* Stop the acquisition if it is still running */
	if (u->in.status == SS_RUNNING) {
		err = ulAInScanStop(u->device_handle);
		if (err != ERR_NO_ERROR)
			return -1;
	}

	//pthread_mutex_unlock(&u->in.mutex);

	err = ulDisconnectDaqDevice(u->device_handle);
	if (err != ERR_NO_ERROR)
		return -1;

	err = ulReleaseDaqDevice(u->device_handle);
	if (err != ERR_NO_ERROR)
		return -1;

	return 0;
}

int uldaq_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct uldaq *u = (struct uldaq *) n->_vd;

	pthread_mutex_lock(&u->in.mutex);


	if (u->in.status != SS_RUNNING)
		return -1;

	long long start_index = u->in.buffer_pos;

	/* Wait for data available condition triggered by event callback */
	if (start_index + n->in.vectorize * u->in.channel_count > u->in.transfer_status.currentScanCount)
		pthread_cond_wait(&u->in.cv, &u->in.mutex);

	for (int j = 0; j < cnt; j++) {
		struct sample *smp = smps[j];

		long long scan_index = start_index + j * u->in.channel_count;

		for (int i = 0; i < u->in.channel_count; i++) {
			long long channel_index = (scan_index + i) % u->in.buffer_len;

			smp->data[i].f = u->in.buffer[channel_index];
		}

		smp->length = u->in.channel_count;
		smp->signals = &n->in.signals;
		smp->sequence = u->sequence++;
		smp->flags = SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_DATA;
	}

	u->in.buffer_pos += u->in.channel_count * cnt;

	pthread_mutex_unlock(&u->in.mutex);

	return cnt;
}

static struct plugin p = {
	.name = "uldaq",
	.description = "Measurement Computing DAQ devices like UL201 (libuldaq)",
	.type = PLUGIN_TYPE_NODE,
	.node = {
		.vectorize	= 0,
		.flags		= 0,
		.size		= sizeof(struct uldaq),
		.type.start	= uldaq_type_start,
		.parse		= uldaq_parse,
		.init		= uldaq_init,
		.destroy	= uldaq_destroy,
		.print		= uldaq_print,
		.start		= uldaq_start,
		.stop		= uldaq_stop,
		.read		= uldaq_read
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
