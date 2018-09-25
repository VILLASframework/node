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
	const char *name,
	Range range
} ranges[] {
	{ "bip60", BIP60VOLTS },	// -60    to +60 Volts
	{ "bip30", BIP30VOLTS },	// -30    to +30 Volts
	{ "bip15", BIP15VOLTS },	// -15    to +15 Volts
	{ "bip20", BIP20VOLTS },	// -20    to +20 Volts
	{ "bip10", BIP10VOLTS },	// -10    to +10 Volts
	{ "bip5", BIP5VOLTS },		// -5     to +5 Volts
	{ "bip4", BIP4VOLTS },		// -4     to +4 Volts
	{ "bip2PT5", BIP2PT5VOLTS },	// -2.5   to +2.5 Volts
	{ "bip2", BIP2VOLTS },		// -2     to +2.0 Volts
	{ "bip1PT25", BIP1PT25VOLTS },	// -1.25  to +1.25 Volts
	{ "bip1", BIP1VOLTS },		// -1     to +1 Volts
	{ "bipPT625", BIPPT625VOLTS },	// -0.625 to +.625 Volts
	{ "bipPT5", BIPPT5VOLTS },	// -0.5   to +.5 Volts
	{ "bipPT25", BIPPT25VOLTS },	// -0.25  to +0.25 Volts
	{ "bipPT125", BIPPT125VOLTS },	// -0.125 to +0.125 Volts
	{ "bipPT2", BIPPT2VOLTS },	// -0.2   to +0.2 Volts
	{ "bipPT1", BIPPT1VOLTS },	// -0.1   to +.1 Volts
	{ "bipPT078", BIPPT078VOLTS },	// -0.078 to +0.078 Volts
	{ "bipPT05", BIPPT05VOLTS },	// -0.05  to +.05 Volts
	{ "bipPT01", BIPPT01VOLTS },	// -0.01  to +.01 Volts
	{ "bipPT005", BIPPT005VOLTS },	// -0.005 to +.005 Volts
	{ "uni60", UNI60VOLTS },	//  0.0   to +60 Volts
	{ "uni30", UNI30VOLTS },	//  0.0   to +30 Volts
	{ "uni15", UNI15VOLTS },	//  0.0   to +15 Volts
	{ "uni20", UNI20VOLTS },	//  0.0   to +20 Volts
	{ "uni10", UNI10VOLTS },	//  0.0   to +10 Volts
	{ "uni5", UNI5VOLTS },		//  0.0   to +5 Volts
	{ "uni4", UNI4VOLTS },		//  0.0   to +4 Volts
	{ "uni2PT5", UNI2PT5VOLTS },	//  0.0   to +2.5 Volts
	{ "uni2", UNI2VOLTS },		//  0.0   to +2.0 Volts
	{ "uni1PT25", UNI1PT25VOLTS },	//  0.0   to +1.25 Volts
	{ "uni1", UNI1VOLTS },		//  0.0   to +1 Volts
	{ "uniPT625", UNIPT625VOLTS },	//  0.0   to +.625 Volts
	{ "uniPT5", UNIPT5VOLTS },	//  0.0   to +.5 Volts
	{ "uniPT25", UNIPT25VOLTS },	//  0.0   to +0.25 Volts
	{ "uniPT125", UNIPT125VOLTS },	//  0.0   to +0.125 Volts
	{ "uniPT2", UNIPT2VOLTS },	//  0.0   to +0.2 Volts
	{ "uniPT1", UNIPT1VOLTS },	//  0.0   to +.1 Volts
	{ "uniPT078", UNIPT078VOLTS },	//  0.0   to +0.078 Volts
	{ "uniPT05", UNIPT05VOLTS },	//  0.0   to +.05 Volts
	{ "uniPT01", UNIPT01VOLTS },	//  0.0   to +.01 Volts
	{ "uniPT005", UNIPT005VOLTS }	//  0.0   to +.005 Volts
};

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

	u->in.queue_len = 0;
	u->in.in.queues = NULL;
	u->in.sample_count = 10000;
	u->in.sample_rate = 1000;
	u->in.scan_options = (ScanOption) (SO_DEFAULTIO | SO_CONTINUOUS);
	u->in.flags = AINSCAN_FF_DEFAULT;
}

int uldaq_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct uldaq *u = (struct uldaq *) n->_vd;

	const char *range = NULL;

	size_t i;
	json_t *json_signals;
	json_t *json_signal;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: { s: o, s: i, s: d } }",
		"in",
			"signals", &json_signals,
			"sample_count", &u->in.sample_count,
			"sample_rate", &u->in.sample_rate,
			"range", &range
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	u->in.queue_len = list_length(&n->in.signals);
	u->in.in.queues = realloc(sizeof(struct AiQueueElement) * u->in.queue_len);

	json_array_foreach(json_signals, i, json_signal) {

	}
	return ret;
}

int uldaq_start(struct node *n)
{
	int ret;
	struct uldaq *u = (struct uldaq *) n->_vd;




	Range ranges[MAX_RANGE_COUNT];
	int numRanges = 0;
	int descriptorIndex = 0;
	unsigned int numDevs = 1;
	UlError err = ERR_NO_ERROR;
	Aiin.InputMode u->in.inputMode = AI_SINGLE_ENDED;
	int chanCount = 1;//change this to use more than one channel
	int index = 0;


	// allocate ain buffer to receive the data
	double *u->in.buffer = (double *) alloc(chanCount * u->sample_count * sizeof(double));
	if (u->in.buffer == 0) {
		warn("Out of memory, unable to create scan u->in.buffer");
		ret = -1;
	}

	// Get descriptors for all of the available DAQ devices
	err = ulGetDaqDeviceInventory(u->interfaceType, u->devDescriptors, &numDevs);

	if (err != ERR_NO_ERROR)
		ret = -1;

	// verify at least one DAQ device is detected
	if (numDevs == 0) {
		warn("No DAQ devices are connected");
		ret = -1;
	}

	// get a handle to the DAQ device associated with the first descriptor
	u->in.daqDeviceHandle = ulCreateDaqDevice(u->devDescriptors[0]);
	if (u->in.daqDeviceHandle == 0) {
		warn ("Unable to create a handle to the specified DAQ device");
		ret = -1;
	}

	// get the analog input ranges
	err = getAiInfoRanges(u->in.daqDeviceHandle, u->in.inputMode, &numRanges, ranges);
	if (err != ERR_NO_ERROR)
		ret = -1;

	err = ulConnectDaqDevice(u->in.daqDeviceHandle);
	if (err != ERR_NO_ERROR)
		ret = -1;

	err = ulAInLoadQueue(u->in.daqDeviceHandle, u->in.queues, chanCount);
	if (err != ERR_NO_ERROR)
		ret = -1;


	Range range; // will be ignored when in queue mode
	int lowChan,highChan; // will be ignored when in queue mode

	// start the acquisition
	//
	// when using the queue, the lowChan, highChan, u->in.inputMode, and range
	// parameters are ignored since they are specified in u->in.queues
	err = ulAInScan(u->in.daqDeviceHandle, lowChan, highChan, u->in.inputMode, range, u->sample_count, &(u->sample_rate), u->scanOptions, u->flags, u->in.buffer);
	if (err == ERR_NO_ERROR) {
		ScanStatus status;
		TransferStatus transferStatus;

		// get the initial status of the acquisition
		ulAInScanStatus(u->in.daqDeviceHandle, &status, &transferStatus);
	}

	if (ret)
		return ret;

	return ret;
}

int uldaq_stop(struct node *n)
{
	int ret;
	struct uldaq *u = (struct uldaq *) n->_vd;

	UlError err = ERR_NO_ERROR;
	ScanStatus status;
	// get the current status of the acquisition
	err = ulAInScanStatus(u->in.daqDeviceHandle, &status, &transferStatus);

	// stop the acquisition if it is still running
	if (status == SS_RUNNING && err == ERR_NO_ERROR)
		ulAInScanStop(u->in.daqDeviceHandle);

	// TODO: error handling
	ulDisconnectDaqDevice(u->in.daqDeviceHandle);
	ulReleaseDaqDevice(u->in.daqDeviceHandle);

	return ret;
}

int uldaq_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int avail;
	struct uldaq *u = (struct uldaq *) n->_vd;

	UlError err = ERR_NO_ERROR;
	ScanStatus status;
	// get the current status of the acquisition
	err = ulAInScanStatus(u->in.daqDeviceHandle, &status, &transferStatus);
	if (status == SS_RUNNING && err == ERR_NO_ERROR) {


		if (err == ERR_NO_ERROR) {
			index = transferStatus.currentIndex;
			int i=0;//we only read one channel
			double currentVal = u->in.buffer[index + i];
		}

	}
	return avail;
}


static struct plugin p = {
	.name = "uldaq",
	.description = "Read USB analog to digital converters like UL201",
	.type = PLUGIN_TYPE_NODE,
	.node = {
		.vectorize = 0,
		.flags	= NODE_TYPE_PROVIDES_SIGNALS,
		.size	= sizeof(struct uldaq),
		.parse	= uldaq_parse,
		.start	= uldaq_start,
		.stop	= uldaq_stop,
		.read	= uldaq_read
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
