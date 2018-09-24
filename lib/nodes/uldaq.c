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
#include <villas/nodes/loopback.h>
#include <villas/memory.h>

static const struct {
	const char *name,
	Range range
} ranges[] {
	{ "BIP60VOLTS", BIP60VOLTS },		// -60 to +60 Volts
	{ "BIP30VOLTS", BIP30VOLTS },		// -30 to +30 Volts
	{ "BIP15VOLTS", BIP15VOLTS },		// -15 to +15 Volts
	{ "BIP20VOLTS", BIP20VOLTS },		// -20 to +20 Volts
	{ "BIP10VOLTS", BIP10VOLTS },		// -10 to +10 Volts
	{ "BIP5VOLTS", BIP5VOLTS },		// -5 to +5 Volts
	{ "BIP4VOLTS", BIP4VOLTS },		// -4 to +4 Volts
	{ "BIP2PT5VOLTS", BIP2PT5VOLTS },	// -2.5 to +2.5 Volts
	{ "BIP2VOLTS", BIP2VOLTS },		// -2.0 to +2.0 Volts
	{ "BIP1PT25VOLTS", BIP1PT25VOLTS },	// -1.25 to +1.25 Volts
	{ "BIP1VOLTS", BIP1VOLTS },		// -1 to +1 Volts
	{ "BIPPT625VOLTS", BIPPT625VOLTS },	// -.625 to +.625 Volts
	{ "BIPPT5VOLTS", BIPPT5VOLTS },		// -.5 to +.5 Volts
	{ "BIPPT25VOLTS", BIPPT25VOLTS },	// -0.25 to +0.25 Volts
	{ "BIPPT125VOLTS", BIPPT125VOLTS },	// -0.125 to +0.125 Volts
	{ "BIPPT2VOLTS", BIPPT2VOLTS },		// -0.2 to +0.2 Volts
	{ "BIPPT1VOLTS", BIPPT1VOLTS },		// -.1 to +.1 Volts
	{ "BIPPT078VOLTS", BIPPT078VOLTS },	// -0.078 to +0.078 Volts
	{ "BIPPT05VOLTS", BIPPT05VOLTS },	// -.05 to +.05 Volts
	{ "BIPPT01VOLTS", BIPPT01VOLTS },	// -.01 to +.01 Volts
	{ "BIPPT005VOLTS", BIPPT005VOLTS },	// -.005 to +.005 Volts
	{ "UNI60VOLTS", UNI60VOLTS },		// 0 to +60 Volts
	{ "UNI30VOLTS", UNI30VOLTS },		// 0 to +30 Volts
	{ "UNI15VOLTS", UNI15VOLTS },		// 0 to +15 Volts
	{ "UNI20VOLTS", UNI20VOLTS },		// 0 to +20 Volts
	{ "UNI10VOLTS", UNI10VOLTS },		// 0 to +10 Volts
	{ "UNI5VOLTS", UNI5VOLTS },		// 0 to +5 Volts
	{ "UNI4VOLTS", UNI4VOLTS },		// 0 to +4 Volts
	{ "UNI2PT5VOLTS", UNI2PT5VOLTS },	// 0 to +2.5 Volts
	{ "UNI2VOLTS", UNI2VOLTS },		// 0 to +2.0 Volts
	{ "UNI1PT25VOLTS", UNI1PT25VOLTS },	// 0 to +1.25 Volts
	{ "UNI1VOLTS", UNI1VOLTS },		// 0 to +1 Volts
	{ "UNIPT625VOLTS", UNIPT625VOLTS },	// 0 to +.625 Volts
	{ "UNIPT5VOLTS", UNIPT5VOLTS },		// 0 to +.5 Volts
	{ "UNIPT25VOLTS", UNIPT25VOLTS },	// 0 to +0.25 Volts
	{ "UNIPT125VOLTS", UNIPT125VOLTS },	// 0 to +0.125 Volts
	{ "UNIPT2VOLTS", UNIPT2VOLTS },		// 0 to +0.2 Volts
	{ "UNIPT1VOLTS", UNIPT1VOLTS },		// 0 to +.1 Volts
	{ "UNIPT078VOLTS", UNIPT078VOLTS },	// 0 to +0.078 Volts
	{ "UNIPT05VOLTS", UNIPT05VOLTS },	// 0 to +.05 Volts
	{ "UNIPT01VOLTS", UNIPT01VOLTS },	// 0 to +.01 Volts
	{ "UNIPT005VOLTS", UNIPT005VOLTS }	// 0 to +.005 Volts
};

static Range uldaq_parse_range(const char *str)
{
}

int uldaq_init(struct node *n)
{
	struct uldaq *u = (struct uldaq *) n->_vd;

	u->in.queue_len = 0;
	u->in.queues = alloc(sizeof(struct AiQueueElement) * u->in.queue_len);

	// set some variables that are used to acquire data
	int u->sample_count = 10000;
	double u->sample_rate = 1000;
	ScanOption u->scanOptions = (ScanOption) (SO_DEFAULTIO | SO_CONTINUOUS);
	AInScanFlag u->flags = AINSCAN_FF_DEFAULT;
}

int uldaq_start(struct node *n)
{
	int ret;
	struct uldaq *u = (struct uldaq *) n->_vd;




	Range ranges[MAX_RANGE_COUNT];
	DaqDeviceDescriptor u->devDescriptors[MAX_DEV_COUNT];
	DaqDeviceInterface u->interfaceType = ANY_IFC;
	DaqDeviceHandle u->daqDeviceHandle = 0;
	int numRanges = 0;
	int descriptorIndex = 0;
	unsigned int numDevs = MAX_DEV_COUNT;
	UlError err = ERR_NO_ERROR;
	AiInputMode u->inputMode = AI_SINGLE_ENDED;
	int chanCount = 1;//change this to use more than one channel
	int index = 0;


	// allocate a buffer to receive the data
	double *buffer = (double*) malloc(chanCount * u->sample_count * sizeof(double));
	if(buffer == 0)
	{
		//printf("\nOut of memory, unable to create scan buffer\n");
		ret = -1;
	}

	// Get descriptors for all of the available DAQ devices
	err = ulGetDaqDeviceInventory(u->interfaceType, u->devDescriptors, &numDevs);

	if(err != ERR_NO_ERROR)
		ret = -1;

	// verify at least one DAQ device is detected
	if (numDevs == 0)
	{
		//printf("No DAQ devices are connected\n");
		ret = -1;
	}

	// get a handle to the DAQ device associated with the first descriptor
	u->daqDeviceHandle = ulCreateDaqDevice(u->devDescriptors[0]);

	if (u->daqDeviceHandle == 0)
	{
		//printf ("\nUnable to create a handle to the specified DAQ device\n");
		ret = -1;
	}

	// get the analog input ranges
	err = getAiInfoRanges(u->daqDeviceHandle, u->inputMode, &numRanges, ranges);

	err = ulConnectDaqDevice(u->daqDeviceHandle);
	if (err != ERR_NO_ERROR)
		ret = -1;


	err = ulAInLoadQueue(u->daqDeviceHandle, u->queues, chanCount);
	if (err != ERR_NO_ERROR)
		ret = -1;


	Range range;//will be ignored when in queue mode
	int lowChan,highChan;//will be ignored when in queue mode
	// start the acquisition
	//
	// when using the queue, the lowChan, highChan, u->inputMode, and range
	// parameters are ignored since they are specified in u->queues
	err = ulAInScan(u->daqDeviceHandle, lowChan, highChan, u->inputMode, range, u->sample_count, &(u->sample_rate), u->scanOptions, u->flags, buffer);


	if(err == ERR_NO_ERROR)
	{
		ScanStatus status;
		TransferStatus transferStatus;

		// get the initial status of the acquisition
		ulAInScanStatus(u->daqDeviceHandle, &status, &transferStatus);
	
	}


	if (ret)
		return ret;

	return queue_signalled_init(&l->queue, l->queuelen, &memory_hugepage, QUEUE_SIGNALLED_EVENTFD);
}

int uldaq_stop(struct node *n)
{
	int ret;
	struct uldaq *u = (struct uldaq *) n->_vd;


	// stop the acquisition if it is still running
	if (status == SS_RUNNING && err == ERR_NO_ERROR)
	{
		ulAInScanStop(u->daqDeviceHandle);
	}

	ulDisconnectDaqDevice(u->daqDeviceHandle);

	ulReleaseDaqDevice(u->daqDeviceHandle);

	if (ret)
		return ret;

	return queue_signalled_destroy(&l->queue);
}

int uldaq_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int avail;
	struct uldaq *u = (struct uldaq *) n->_vd;

	if(status == SS_RUNNING && err == ERR_NO_ERROR)
	{
		// get the current status of the acquisition
		err = ulAInScanStatus(u->daqDeviceHandle, &status, &transferStatus);

		if(err == ERR_NO_ERROR)
		{
			index = transferStatus.currentIndex;
			int i=0;//we only read one channel
			double currentVal = buffer[index + i];
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
		.u->flags	= NODE_TYPE_PROVIDES_SIGNALS,
		.size	= sizeof(struct uldaq),
		.parse	= loopback_parse,
		.print	= loopback_print,
		.start	= uldaq_start,
		.stop	= uldaq_stop,
		.read	= uldaq_read
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
