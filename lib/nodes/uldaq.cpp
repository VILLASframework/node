/** Node-type for uldaq connections.
 *
 * @file
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstring>

#include <villas/config.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/uldaq.hpp>
#include <villas/exceptions.hpp>
#include <villas/node/memory.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

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

static
AiInputMode uldaq_parse_input_mode(const char *str)
{
	for (unsigned i = 0; i < ARRAY_LEN(input_modes); i++) {
		if (!strcmp(input_modes[i].name, str))
			return input_modes[i].mode;
	}

	return (AiInputMode) -1;
}

static
DaqDeviceInterface uldaq_parse_interface_type(const char *str)
{
	for (unsigned i = 0; i < ARRAY_LEN(interface_types); i++) {
		if (!strcmp(interface_types[i].name, str))
			return interface_types[i].interface;
	}

	return (DaqDeviceInterface) -1;
}

static
const char * uldaq_print_interface_type(DaqDeviceInterface iftype)
{
	for (unsigned i = 0; i < ARRAY_LEN(interface_types); i++) {
		if (interface_types[i].interface == iftype)
			return interface_types[i].name;
	}

	return nullptr;
}

static
Range uldaq_parse_range(const char *str)
{
	for (unsigned i = 0; i < ARRAY_LEN(ranges); i++) {
		if (!strcmp(ranges[i].name, str))
			return ranges[i].range;
	}

	return (Range) -1;
}

static
DaqDeviceDescriptor * uldaq_find_device(struct uldaq *u) {
	DaqDeviceDescriptor *d = nullptr;

	if (num_devs == 0)
		return nullptr;

	if (u->device_interface_type == ANY_IFC && u->device_id == nullptr)
		return &descriptors[0];

	for (unsigned i = 0; i < num_devs; i++) {
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

	return nullptr;
}

static
int uldaq_connect(NodeCompat *n)
{
	auto *u = n->getData<struct uldaq>();
	UlError err;

	/* Find Matching device */
	if (!u->device_descriptor) {
		u->device_descriptor = uldaq_find_device(u);
		if (!u->device_descriptor)
			throw RuntimeError("Unable to find a matching device");
	}

	/* Get a handle to the DAQ device associated with the first descriptor */
	if (!u->device_handle) {
		u->device_handle = ulCreateDaqDevice(*u->device_descriptor);
		if (!u->device_handle)
			throw RuntimeError("Unable to create handle for DAQ device");
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
			char buf[ERR_MSG_LEN];
			ulGetErrMsg(err, buf);
			throw RuntimeError("Failed to connect to DAQ device: {}", buf);
		}
	}

	return 0;
}

int villas::node::uldaq_type_start(villas::node::SuperNode *sn)
{
	UlError err;

	/* Get descriptors for all of the available DAQ devices */
	err = ulGetDaqDeviceInventory(ANY_IFC, descriptors, &num_devs);
	if (err != ERR_NO_ERROR)
		throw RuntimeError("Failed to retrieve DAQ device list");

	auto logger = logging.get("node:uldaq");
	logger->info("Found {} DAQ devices", num_devs);
	for (unsigned i = 0; i < num_devs; i++) {
		DaqDeviceDescriptor *desc = &descriptors[i];

		logger->info("  {}: {} {} ({})", i, desc->uniqueId, desc->devString,  uldaq_print_interface_type(desc->devInterface));
	}

	return 0;
}

int villas::node::uldaq_init(NodeCompat *n)
{
	int ret;
	auto *u = n->getData<struct uldaq>();

	u->device_id = nullptr;
	u->device_interface_type = ANY_IFC;

	u->in.queues = nullptr;
	u->in.sample_rate = 1000;
	u->in.scan_options = (ScanOption) (SO_DEFAULTIO | SO_CONTINUOUS);
	u->in.flags = AINSCAN_FF_DEFAULT;

	ret = pthread_mutex_init(&u->in.mutex, nullptr);
	if (ret)
		return ret;

	ret = pthread_cond_init(&u->in.cv, nullptr);
	if (ret)
		return ret;

	return 0;
}

int villas::node::uldaq_destroy(NodeCompat *n)
{
	int ret;
	auto *u = n->getData<struct uldaq>();

	if (u->in.queues)
		delete[] u->in.queues;

	ret = pthread_mutex_destroy(&u->in.mutex);
	if (ret)
		return ret;

	ret = pthread_cond_destroy(&u->in.cv);
	if (ret)
		return ret;

	return 0;
}

int villas::node::uldaq_parse(NodeCompat *n, json_t *json)
{
	int ret;
	auto *u = n->getData<struct uldaq>();

	const char *default_range_str = nullptr;
	const char *default_input_mode_str = nullptr;
	const char *interface_type = nullptr;

	size_t i;
	json_t *json_signals;
	json_t *json_signal;
	json_error_t err;

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s: { s: o, s: F, s?: s, s?: s } }",
		"interface_type", &interface_type,
		"device_id", &u->device_id,
		"in",
			"signals", &json_signals,
			"sample_rate", &u->in.sample_rate,
			"range", &default_range_str,
			"input_mode", &default_input_mode_str
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-uldaq");

	if (interface_type) {
		int iftype = uldaq_parse_interface_type(interface_type);
		if (iftype < 0)
			throw ConfigError(json, "node-config-node-uldaq-interface-type", "Invalid interface type: {}", interface_type);

		u->device_interface_type = (DaqDeviceInterface) iftype;
	}

	if (u->in.queues)
		delete[] u->in.queues;

	u->in.channel_count = n->getInputSignals(false)->size();
	u->in.queues = new struct AiQueueElement[u->in.channel_count];
	if (!u->in.queues)
		throw MemoryAllocationError();

	json_array_foreach(json_signals, i, json_signal) {
		const char *range_str = nullptr, *input_mode_str = nullptr;
		int channel = -1, input_mode, range;

		ret = json_unpack_ex(json_signal, &err, 0, "{ s?: s, s?: s, s?: i }",
			"range", &range_str,
			"input_mode", &input_mode_str,
			"channel", &channel
		);
		if (ret)
			throw ConfigError(json_signal, err, "node-config-node-uldaq-signal", "Failed to parse signal configuration");

		if (!range_str)
			range_str = default_range_str;

		if (!input_mode_str)
			input_mode_str = default_input_mode_str;

		if (channel < 0)
			channel = i;

		if (!range_str)
			throw ConfigError(json_signal, err, "node-config-node-uldaq-signal", "No input range specified for signal {}.", i);

		if (!input_mode_str)
			throw ConfigError(json_signal, err, "node-config-node-uldaq-signal", "No input mode specified for signal {}.", i);

		range = uldaq_parse_range(range_str);
		if (range < 0)
			throw ConfigError(json_signal, err, "node-config-node-uldaq-signal", "Invalid input range specified for signal {}.", i);

		input_mode = uldaq_parse_input_mode(input_mode_str);
		if (input_mode < 0)
			throw ConfigError(json_signal, err, "node-config-node-uldaq-signal", "Invalid input mode specified for signal {}.", i);

		u->in.queues[i].range = (Range) range;
		u->in.queues[i].inputMode = (AiInputMode) input_mode;
		u->in.queues[i].channel = channel;
	}

	return ret;
}

char * villas::node::uldaq_print(NodeCompat *n)
{
	auto *u = n->getData<struct uldaq>();

	char *buf = nullptr;

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

int villas::node::uldaq_check(NodeCompat *n)
{
	int ret;
	long long has_ai, event_types, max_channel, scan_options, num_ranges_se, num_ranges_diff;
	auto *u = n->getData<struct uldaq>();

	UlError err;

	if (n->in.vectorize < 100)
		throw ConfigError(n->getConfig(), "node-config-node-vectorize", "Setting 'vectorize' must be larger than 100");

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

	if (!has_ai)
		throw RuntimeError("DAQ device has no analog input channels");

	if (!(event_types & DE_ON_DATA_AVAILABLE))
		throw RuntimeError("DAQ device does not support events");

	if ((scan_options & u->in.scan_options) != u->in.scan_options)
		throw RuntimeError("DAQ device does not support required scan options");

	for (size_t i = 0; i < n->getInputSignals(false)->size(); i++) {
		auto sig = n->getInputSignals(false)->getByIndex(i);
		AiQueueElement *q = &u->in.queues[i];

		if (sig->type != SignalType::FLOAT)
			throw RuntimeError("Node supports only signals of type = float!");

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

		throw RuntimeError("Unsupported range for signal {}", i);

found:		if (q->channel > max_channel)
			throw RuntimeError("DAQ device does not support more than {} channels", max_channel);
	}

	return 0;
}

static
void uldaq_data_available(DaqDeviceHandle device_handle, DaqEventType event_type, unsigned long long event_data, void *ctx)
{
	auto *n = (NodeCompat *) ctx;
	auto *u = n->getData<struct uldaq>();

	pthread_mutex_lock(&u->in.mutex);

	UlError err;
	err = ulAInScanStatus(device_handle, &u->in.status, &u->in.transfer_status);
	if (err != ERR_NO_ERROR)
		n->logger->warn("Failed to retrieve scan status in event callback");

	pthread_mutex_unlock(&u->in.mutex);

	/* Signal uldaq_read() about new data */
	pthread_cond_signal(&u->in.cv);
}

int villas::node::uldaq_start(NodeCompat *n)
{
	auto *u = n->getData<struct uldaq>();

	u->sequence = 0;
	u->in.buffer_pos = 0;

	int ret;
	UlError err;

	/* Allocate a buffer to receive the data */
	u->in.buffer_len = u->in.channel_count * n->in.vectorize * 50;
	u->in.buffer = new double[u->in.buffer_len];
	if (!u->in.buffer)
		throw MemoryAllocationError();

	ret = uldaq_connect(n);
	if (ret)
		return ret;

	err = ulAInLoadQueue(u->device_handle, u->in.queues, n->getInputSignals(false)->size());
	if (err != ERR_NO_ERROR)
		throw RuntimeError("Failed to load input queue to DAQ device");

	/* Enable the event to be notified every time samples are available */
	err = ulEnableEvent(u->device_handle, DE_ON_DATA_AVAILABLE, n->in.vectorize, uldaq_data_available, n);

	/* Start the acquisition */
	err = ulAInScan(u->device_handle, 0, 0, (AiInputMode) 0, (Range) 0, u->in.buffer_len / u->in.channel_count, &u->in.sample_rate, u->in.scan_options, u->in.flags, u->in.buffer);
	if (err != ERR_NO_ERROR) {
		char buf[ERR_MSG_LEN];
		ulGetErrMsg(err, buf);
		throw RuntimeError("Failed to start acquisition on DAQ device: {}", buf);
	}

	/* Get the initial status of the acquisition */
	err = ulAInScanStatus(u->device_handle, &u->in.status, &u->in.transfer_status);
	if (err != ERR_NO_ERROR) {
		char buf[ERR_MSG_LEN];
		ulGetErrMsg(err, buf);
		throw RuntimeError("Failed to retrieve scan status on DAQ device: {}", buf);
	}

	if (u->in.status != SS_RUNNING) {
		char buf[ERR_MSG_LEN];
		ulGetErrMsg(err, buf);
		throw RuntimeError("Acquisition did not start on DAQ device: {}", buf);
	}

	return 0;
}

int villas::node::uldaq_stop(NodeCompat *n)
{
	auto *u = n->getData<struct uldaq>();

	UlError err;

	/* @todo Fix deadlock */
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

int villas::node::uldaq_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	auto *u = n->getData<struct uldaq>();

	pthread_mutex_lock(&u->in.mutex);

	if (u->in.status != SS_RUNNING)
		return -1;

	size_t start_index = u->in.buffer_pos;

	/* Wait for data available condition triggered by event callback */
	if (start_index + n->in.vectorize * u->in.channel_count > u->in.transfer_status.currentTotalCount)
		pthread_cond_wait(&u->in.cv, &u->in.mutex);

	for (unsigned j = 0; j < cnt; j++) {
		struct Sample *smp = smps[j];

		long long scan_index = start_index + j * u->in.channel_count;

		for (unsigned i = 0; i < u->in.channel_count; i++) {
			long long channel_index = (scan_index + i) % u->in.buffer_len;

			smp->data[i].f = u->in.buffer[channel_index];
		}

		smp->length = u->in.channel_count;
		smp->signals = n->getInputSignals(false);
		smp->sequence = u->sequence++;
		smp->flags = (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA;
	}

	u->in.buffer_pos += u->in.channel_count * cnt;

	pthread_mutex_unlock(&u->in.mutex);

	return cnt;
}

static NodeCompatType p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "uldaq";
	p.description	= "Measurement Computing DAQ devices like UL201 (libuldaq)";
	p.vectorize	= 0;
	p.flags		= 0;
	p.size		= sizeof(struct uldaq);
	p.type.start	= uldaq_type_start;
	p.init		= uldaq_init;
	p.destroy	= uldaq_destroy;
	p.parse		= uldaq_parse;
	p.print		= uldaq_print;
	p.start		= uldaq_start;
	p.stop		= uldaq_stop;
	p.read		= uldaq_read;

	static NodeCompatFactory ncp(&p);
}
