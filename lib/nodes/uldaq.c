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
	Range range;
} ranges[] = {
	{ "bipolar-60", BIP60VOLTS },		// -60    to +60 Volts
	{ "bipolar-30", BIP30VOLTS },		// -30    to +30 Volts
	{ "bipolar-15", BIP15VOLTS },		// -15    to +15 Volts
	{ "bipolar-20", BIP20VOLTS },		// -20    to +20 Volts
	{ "bipolar-10", BIP10VOLTS },		// -10    to +10 Volts
	{ "bipolar-5", BIP5VOLTS },		// -5     to +5 Volts
	{ "bipolar-4", BIP4VOLTS },		// -4     to +4 Volts
	{ "bipolar-2.5", BIP2PT5VOLTS },	// -2.5   to +2.5 Volts
	{ "bipolar-2", BIP2VOLTS },		// -2     to +2.0 Volts
	{ "bipolar-1.25", BIP1PT25VOLTS },	// -1.25  to +1.25 Volts
	{ "bipolar-1", BIP1VOLTS },		// -1     to +1 Volts
	{ "bipolar-0.625", BIPPT625VOLTS },	// -0.625 to +.625 Volts
	{ "bipolar-0.5", BIPPT5VOLTS },		// -0.5   to +.5 Volts
	{ "bipolar-0.25", BIPPT25VOLTS },	// -0.25  to +0.25 Volts
	{ "bipolar-0.125", BIPPT125VOLTS },	// -0.125 to +0.125 Volts
	{ "bipolar-0.2", BIPPT2VOLTS },		// -0.2   to +0.2 Volts
	{ "bipolar-0.1", BIPPT1VOLTS },		// -0.1   to +.1 Volts
	{ "bipolar-0.078", BIPPT078VOLTS },	// -0.078 to +0.078 Volts
	{ "bipolar-0.05", BIPPT05VOLTS },	// -0.05  to +.05 Volts
	{ "bipolar-0.01", BIPPT01VOLTS },	// -0.01  to +.01 Volts
	{ "bipolar-0.005", BIPPT005VOLTS },	// -0.005 to +.005 Volts
	{ "unipolar-60", UNI60VOLTS },		//  0.0   to +60 Volts
	{ "unipolar-30", UNI30VOLTS },		//  0.0   to +30 Volts
	{ "unipolar-15", UNI15VOLTS },		//  0.0   to +15 Volts
	{ "unipolar-20", UNI20VOLTS },		//  0.0   to +20 Volts
	{ "unipolar-10", UNI10VOLTS },		//  0.0   to +10 Volts
	{ "unipolar-5", UNI5VOLTS },		//  0.0   to +5 Volts
	{ "unipolar-4", UNI4VOLTS },		//  0.0   to +4 Volts
	{ "unipolar-2.5", UNI2PT5VOLTS },	//  0.0   to +2.5 Volts
	{ "unipolar-2", UNI2VOLTS },		//  0.0   to +2.0 Volts
	{ "unipolar-1.25", UNI1PT25VOLTS },	//  0.0   to +1.25 Volts
	{ "unipolar-1", UNI1VOLTS },		//  0.0   to +1 Volts
	{ "unipolar-0.625", UNIPT625VOLTS },	//  0.0   to +.625 Volts
	{ "unipolar-0.5", UNIPT5VOLTS },	//  0.0   to +.5 Volts
	{ "unipolar-0.25", UNIPT25VOLTS },	//  0.0   to +0.25 Volts
	{ "unipolar-0.125", UNIPT125VOLTS },	//  0.0   to +0.125 Volts
	{ "unipolar-0.2", UNIPT2VOLTS },	//  0.0   to +0.2 Volts
	{ "unipolar-0.1", UNIPT1VOLTS },	//  0.0   to +.1 Volts
	{ "unipolar-0.078", UNIPT078VOLTS },	//  0.0   to +0.078 Volts
	{ "unipolar-0.05", UNIPT05VOLTS },	//  0.0   to +.05 Volts
	{ "unipolar-0.01", UNIPT01VOLTS },	//  0.0   to +.01 Volts
	{ "unipolar-0.005", UNIPT005VOLTS }	//  0.0   to +.005 Volts
};

static UlError uldag_range_info(DaqDeviceHandle daqDeviceHandle, AiInputMode inputMode, int *numberOfRanges, Range* ranges)
{
	UlError err = ERR_NO_ERROR;
	int i = 0;
	long long numRanges = 0;
	long long rng;

	if (inputMode == AI_SINGLE_ENDED)
	{
		err = ulAIGetInfo(daqDeviceHandle, AI_INFO_NUM_SE_RANGES, 0, &numRanges);
	}
	else
	{
		err = ulAIGetInfo(daqDeviceHandle, AI_INFO_NUM_DIFF_RANGES, 0, &numRanges);
	}

	for (i=0; i<numRanges; i++)
	{
		if (inputMode == AI_SINGLE_ENDED)
		{
			err = ulAIGetInfo(daqDeviceHandle, AI_INFO_SE_RANGE, i, &rng);
		}
		else
		{
			err = ulAIGetInfo(daqDeviceHandle, AI_INFO_DIFF_RANGE, i, &rng);
		}

		ranges[i] = (Range)rng;
	}

	*numberOfRanges = (int)numRanges;

	return err;
}

__attribute__((unused))
static Range uldaq_range_parse(const char *str)
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

	const char *range = NULL;

	size_t i;
	json_t *json_signals;
	json_t *json_signal;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: { s: o, s: F } }",
		"in",
			"signals", &json_signals,
			"sample_rate", &u->in.sample_rate,
			"range", &range
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	u->in.queues = realloc(u->in.queues, sizeof(struct AiQueueElement) * list_length(&n->signals));

	json_array_foreach(json_signals, i, json_signal) {

	}

	return ret;
}


char * uldaq_print(struct node *n)
{
	return strf("TODO");
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
	Range ranges[ULDAQ_MAX_RANGE_COUNT];

	UlError err;
	unsigned num_devs;
	int num_ranges;

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

	// verify at least one DAQ device is detected
	if (num_devs == 0) {
		warn("No DAQ devices are connected");
		return -1;
	}

	// get a handle to the DAQ device associated with the first descriptor
	u->device_handle = ulCreateDaqDevice(descriptors[0]);
	if (u->device_handle == 0) {
		warn ("Unable to create a handle to the specified DAQ device");
		return -1;
	}

	// get the analog input ranges
	err = uldag_range_info(u->device_handle, u->in.input_mode, &num_ranges, ranges);
	if (err != ERR_NO_ERROR)
		return -1;

	err = ulConnectDaqDevice(u->device_handle);
	if (err != ERR_NO_ERROR)
		return -1;

	err = ulAInLoadQueue(u->device_handle, u->in.queues, list_length(&n->signals));
	if (err != ERR_NO_ERROR)
		return -1;

	// start the acquisition
	// when using the queue, the lowChan, highChan, u->in.input_mode, and range
	// parameters are ignored since they are specified in u->queues
	err = ulAInScan(u->device_handle, 0, 0, u->in.input_mode, 0, u->in.sample_count, &(u->in.sample_rate), u->in.scan_options, u->in.flags, u->in.buffer);
	if (err == ERR_NO_ERROR) {
		ScanStatus status;
		TransferStatus transferStatus;

		// get the initial status of the acquisition
		ulAInScanStatus(u->device_handle, &status, &transferStatus);
	}

	return 0;
}

int uldaq_stop(struct node *n)
{
	struct uldaq *u = (struct uldaq *) n->_vd;

	UlError err = ERR_NO_ERROR;
	ScanStatus status;
	TransferStatus transferStatus;
	// get the current status of the acquisition
	err = ulAInScanStatus(u->device_handle, &status, &transferStatus);

	// stop the acquisition if it is still running
	if (status == SS_RUNNING && err == ERR_NO_ERROR)
		ulAInScanStop(u->device_handle);

	// TODO: error handling
	ulDisconnectDaqDevice(u->device_handle);
	ulReleaseDaqDevice(u->device_handle);

	return 0;
}

int uldaq_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct uldaq *u = (struct uldaq *) n->_vd;

	UlError err = ERR_NO_ERROR;
	ScanStatus status;
	TransferStatus transferStatus;
	// get the current status of the acquisition
	err = ulAInScanStatus(u->device_handle, &status, &transferStatus);
	if (status == SS_RUNNING && err == ERR_NO_ERROR) {
		if (err == ERR_NO_ERROR) {
			//int index = transferStatus.currentIndex;
			//int i=0;//we only read one channel
			//double currentVal = u->in.buffer[index + i];
		}
	}

	return 0;
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
