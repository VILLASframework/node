/** Node-type for uldaq connections.
 *
 * @file
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
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

#include <villas/node.h>
#include <villas/plugin.h>
#include <villas/config.h>
#include <villas/nodes/uldaq.h>
#include <villas/memory.h>

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

__attribute__((unused))
static UlError uldag_range_info(DaqDeviceHandle device_handle, AiInputMode input_mode, int *number_of_ranges, Range* ranges)
{
	UlError err;
	long long num_ranges = 0;
	long long range;

	err = input_mode == AI_SINGLE_ENDED
		? ulAIGetInfo(device_handle, AI_INFO_NUM_SE_RANGES, 0, &num_ranges)
		: ulAIGetInfo(device_handle, AI_INFO_NUM_DIFF_RANGES, 0, &num_ranges);
	if (err != ERR_NO_ERROR)
		return err;

	for (int i = 0; i < num_ranges; i++) {
		err = input_mode == AI_SINGLE_ENDED
			? ulAIGetInfo(device_handle, AI_INFO_SE_RANGE, i, &range)
			: ulAIGetInfo(device_handle, AI_INFO_DIFF_RANGE, i, &range);
		if (err != ERR_NO_ERROR)
			return err;

		ranges[i] = (Range) range;
	}

	*number_of_ranges = (int) num_ranges;

	return ERR_NO_ERROR;
}

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

int uldaq_init(struct node *n)
{
	struct uldaq *u = (struct uldaq *) n->_vd;

	u->device_interface_type = ANY_IFC;

	u->in.queues = NULL;
	u->in.sample_rate = 1000;
	u->in.scan_options = (ScanOption) (SO_DEFAULTIO | SO_CONTINUOUS);
	u->in.flags = AINSCAN_FF_DEFAULT;

	return 0;
}

int uldaq_destroy(struct node *n)
{
	struct uldaq *u = (struct uldaq *) n->_vd;

	if (u->in.queues)
		free(u->in.queues);

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

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s: { s: o, s: F, s?: s, s?: s } }",
		"interface_type", &interface_type,
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

	u->in.queues = realloc(u->in.queues, sizeof(struct AiQueueElement) * list_length(&n->signals));

	json_array_foreach(json_signals, i, json_signal) {
		const char *range_str = NULL, *input_mode_str = NULL;
		int channel = -1, input_mode, range;

		json_unpack_ex(json_signal, &err, 0, "{ s?: s, s?: s, s?: i }",
			"range", &range,
			"input_mode", &input_mode,
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
	char *uid = u->device_descriptor.uniqueId;
	char *name = u->device_descriptor.productName;
	const char *iftype = uldaq_print_interface_type(u->device_interface_type);

	buf = strcatf(&buf, "device=%s (%s), interface=%s", uid, name, iftype);
	buf = strcatf(&buf, ", in.sample_rate=%f", u->in.sample_rate);

	return buf;
}

int uldaq_check(struct node *n)
{
	struct uldaq *u = (struct uldaq *) n->_vd;

	(void) u; // unused for now

	if (n->in.vectorize < 100) {
		warn("vectorize setting of node '%s' must be larger than 100", node_name(n));
		return -1;
	}

	for (size_t i = 0; i < list_length(&n->signals); i++) {
		struct signal *s = (struct signal *) list_at(&n->signals, i);

		if (s->type != SIGNAL_TYPE_FLOAT) {
			warn("Node '%s' only supports signals of type = float!", node_name(n));
			return -1;
		}
	}

	return 0;
}

int uldaq_start(struct node *n)
{
	struct uldaq *u = (struct uldaq *) n->_vd;

	DaqDeviceDescriptor descriptors[ULDAQ_MAX_DEV_COUNT];

	UlError err;
	unsigned num_devs;

	// allocate a buffer to receive the data
	u->in.buffer = (double *) alloc(list_length(&n->signals) * n->in.vectorize * sizeof(double));
	if (u->in.buffer == 0) {
		warn("Out of memory, unable to create scan buffer");
		return -1;
	}

	// Get descriptors for all of the available DAQ devices
	err = ulGetDaqDeviceInventory(u->device_interface_type, descriptors, &num_devs);
	if (err != ERR_NO_ERROR)
		return -1;

	/* Verify at least one DAQ device is detected */
	if (num_devs == 0) {
		warn("No DAQ devices are connected");
		return -1;
	}

	/* Get a handle to the DAQ device associated with the first descriptor */
	u->device_handle = ulCreateDaqDevice(descriptors[0]);
	if (u->device_handle == 0) {
		warn ("Unable to create a handle to the specified DAQ device");
		return -1;
	}

#if 0
	int num_ranges;
	Range ranges[ULDAQ_MAX_RANGE_COUNT];
	/* Get the analog input ranges */
	err = uldag_range_info(u->device_handle, u->in.input_mode, &num_ranges, ranges);
	if (err != ERR_NO_ERROR)
		return -1;
#endif

	err = ulConnectDaqDevice(u->device_handle);
	if (err != ERR_NO_ERROR)
		return -1;

	err = ulAInLoadQueue(u->device_handle, u->in.queues, list_length(&n->signals));
	if (err != ERR_NO_ERROR)
		return -1;

	/* Start the acquisition */
	err = ulAInScan(u->device_handle, 0, 0, 0, 0, n->in.vectorize, &u->in.sample_rate, u->in.scan_options, u->in.flags, u->in.buffer);
	if (err != ERR_NO_ERROR)
		return -1;

	ScanStatus status;
	TransferStatus transfer_status;

	/* Get the initial status of the acquisition */
	ulAInScanStatus(u->device_handle, &status, &transfer_status);

	if (status != SS_RUNNING)
		return -1;

	return 0;
}

int uldaq_stop(struct node *n)
{
	struct uldaq *u = (struct uldaq *) n->_vd;

	UlError err;
	ScanStatus status;
	TransferStatus transferStatus;

	/* Get the current status of the acquisition */
	err = ulAInScanStatus(u->device_handle, &status, &transferStatus);
	if (err != ERR_NO_ERROR)
		return -1;

	/* Stop the acquisition if it is still running */
	if (status == SS_RUNNING) {
		err = ulAInScanStop(u->device_handle);
		if (err != ERR_NO_ERROR)
			return -1;
	}

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

	UlError err;
	ScanStatus status;
	TransferStatus transferStatus;

	/* Get the current status of the acquisition */
	err = ulAInScanStatus(u->device_handle, &status, &transferStatus);
	if (err != ERR_NO_ERROR)
		return -1;

	if (status == SS_RUNNING) {
		int index = transferStatus.currentIndex;

		struct sample *smp = smps[0];

		for (int i = 0; i < list_length(&n->signals); i++) {
			smp->data[i].f = u->in.buffer[index + i];
		}
	}

	return 1;
}


static struct plugin p = {
	.name = "uldaq",
	.description = "Read USB analog to digital converters like UL201",
	.type = PLUGIN_TYPE_NODE,
	.node = {
		.vectorize = 0,
		.flags	= 0,
		.size	= sizeof(struct uldaq),
		.parse	= uldaq_parse,
		.init	= uldaq_init,
		.destroy= uldaq_destroy,
		.print	= uldaq_print,
		.start	= uldaq_start,
		.stop	= uldaq_stop,
		.read	= uldaq_read
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
