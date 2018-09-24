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

int uldaq_start(struct node *n)
{
	int ret;

    Range ranges[MAX_RANGE_COUNT];
    DaqDeviceDescriptor devDescriptors[MAX_DEV_COUNT];
    DaqDeviceInterface interfaceType = ANY_IFC;
    DaqDeviceHandle daqDeviceHandle = 0;
    int numRanges = 0;
    int descriptorIndex = 0;
    unsigned int numDevs = MAX_DEV_COUNT;
    UlError err = ERR_NO_ERROR;
    AiInputMode inputMode = AI_SINGLE_ENDED;
    int samplesPerChannel = 10000;
    int chanCount = 1;//change this to use more than one channel
    ScanOption scanOptions = (ScanOption) (SO_DEFAULTIO | SO_CONTINUOUS);
    AInScanFlag flags = AINSCAN_FF_DEFAULT;
    int index = 0;
    AiQueueElement queueArray[MAX_QUEUE_SIZE];


	// allocate a buffer to receive the data
	double *buffer = (double*) malloc(chanCount * samplesPerChannel * sizeof(double));
	if(buffer == 0)
	{
		//printf("\nOut of memory, unable to create scan buffer\n");
		ret = -1;
	}

	// Get descriptors for all of the available DAQ devices
	err = ulGetDaqDeviceInventory(interfaceType, devDescriptors, &numDevs);

	if(err != ERR_NO_ERROR)
        ret = -1;

	// verify at least one DAQ device is detected
	if (numDevs == 0)
	{
		//printf("No DAQ devices are connected\n");
		ret = -1;
	}

	// get a handle to the DAQ device associated with the first descriptor
	daqDeviceHandle = ulCreateDaqDevice(devDescriptors[0]);

	if (daqDeviceHandle == 0)
	{
		//printf ("\nUnable to create a handle to the specified DAQ device\n");
		ret = -1;
	}

	// get the analog input ranges
	err = getAiInfoRanges(daqDeviceHandle, inputMode, &numRanges, ranges);

	err = ulConnectDaqDevice(daqDeviceHandle);
	if (err != ERR_NO_ERROR)
		ret = -1;


    
	for (i=0; i<chanCount; i++)
	{
		queueArray[i].channel = i;
		queueArray[i].inputMode = inputMode;
		queueArray[i].range = ranges[i % numRanges];
	}

	err = ulAInLoadQueue(daqDeviceHandle, queueArray, chanCount);
	if (err != ERR_NO_ERROR)
		ret = -1;


    Range range;//will be ignored when in queue mode
    int lowChan,highChan;//will be ignored when in queue mode
	// start the acquisition
	//
	// when using the queue, the lowChan, highChan, inputMode, and range
	// parameters are ignored since they are specified in queueArray
    err = ulAInScan(daqDeviceHandle, lowChan, highChan, inputMode, range, samplesPerChannel, &rate, scanOptions, flags, buffer);


	if(err == ERR_NO_ERROR)
	{
		ScanStatus status;
		TransferStatus transferStatus;

		// get the initial status of the acquisition
		ulAInScanStatus(daqDeviceHandle, &status, &transferStatus);
    
    }


	if (ret)
		return ret;

	return queue_signalled_init(&l->queue, l->queuelen, &memory_hugepage, QUEUE_SIGNALLED_EVENTFD);
}

int uldaq_stop(struct node *n)
{
	int ret;


    // stop the acquisition if it is still running
    if (status == SS_RUNNING && err == ERR_NO_ERROR)
    {
        ulAInScanStop(daqDeviceHandle);
    }

    ulDisconnectDaqDevice(daqDeviceHandle);

    ulReleaseDaqDevice(daqDeviceHandle);

	if (ret)
		return ret;

	return queue_signalled_destroy(&l->queue);
}

int uldaq_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int avail;

    if(status == SS_RUNNING && err == ERR_NO_ERROR)
    {
        // get the current status of the acquisition
        err = ulAInScanStatus(daqDeviceHandle, &status, &transferStatus);

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
		.flags	= NODE_TYPE_PROVIDES_SIGNALS,
		.size	= sizeof(struct loopback),
		.parse	= loopback_parse,
		.print	= loopback_print,
		.start	= uldaq_start,
		.stop	= uldaq_stop,
		.read	= uldaq_read
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)