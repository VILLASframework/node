/* Node-type for uldaq connections.
 *
 * Author: Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <pthread.h>

#include <villas/queue_signalled.h>
#include <villas/pool.hpp>

#include <uldaq.h>

#define ULDAQ_MAX_DEV_COUNT 100
#define ULDAQ_MAX_RANGE_COUNT 8

namespace villas {
namespace node {

struct uldaq {
	const char *device_id;

	DaqDeviceHandle device_handle;
	DaqDeviceDescriptor *device_descriptor;
	DaqDeviceInterface device_interface_type;

	uint64_t sequence;

	struct {
		double sample_rate;
		double *buffer;
		size_t buffer_len;
		size_t buffer_pos;
		size_t channel_count;

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

int uldaq_type_start(SuperNode *sn);

int uldaq_init(NodeCompat *n);

int uldaq_destroy(NodeCompat *n);

int uldaq_parse(NodeCompat *n, json_t *json);

char * uldaq_print(NodeCompat *n);

int uldaq_check(NodeCompat *n);

int uldaq_start(NodeCompat *n);

int uldaq_stop(NodeCompat *n);

int uldaq_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} // namespace node
} // namespace villas
