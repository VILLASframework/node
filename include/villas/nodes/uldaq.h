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

/**
 * @ingroup node
 * @addtogroup uldaq Read USB analog to digital converters
 * @{
 */

#pragma once

#include <pthread.h>

#include <villas/queue_signalled.h>
#include <villas/pool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <uldaq.h>

#define ULDAQ_MAX_DEV_COUNT 100
#define ULDAQ_MAX_RANGE_COUNT 8

struct uldaq {
	DaqDeviceHandle device_handle;
	DaqDeviceDescriptor device_descriptor;
	DaqDeviceInterface device_interface_type;

	struct {
		double sample_rate;
		double *buffer;
		unsigned buffer_len;
		unsigned channel_count;

		ScanOption scan_options;
		AInScanFlag flags;
		AiQueueElement *queues;
		ScanStatus status; // protected by mutex
		TransferStatus transfer_status; // protected by mutex

		pthread_mutex_t mutex;
		pthread_cond_t cv;
	} in;

	struct {
		// TODO
	} out;
};

#ifdef __cplusplus
}
#endif

/** @} */
