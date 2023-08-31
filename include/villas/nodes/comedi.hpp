/* Node type: comedi
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <comedilib.h>

#include <villas/list.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;

// whether to use read() or mmap() kernel interface
#define COMEDI_USE_READ (1)
//#define COMEDI_USE_READ (0)

struct comedi_chanspec {
	unsigned int maxdata;
	comedi_range *range;
};

struct comedi_direction {
	int subdevice;				///< Comedi subdevice
	int buffer_size;			///< Comedi's kernel buffer size in kB
	int sample_size;			///< Size of a single measurement sample
	int sample_rate_hz;			///< Sample rate in Hz
	bool present;				///< Config present
	bool enabled;				///< Card is started successfully
	bool running;				///< Card is actively transfering samples
	struct timespec started;		///< Timestamp when sampling started
	struct timespec last_debug;		///< Timestamp of last debug output
	size_t counter;				///< Number of villas samples transfered
	struct comedi_chanspec *chanspecs;	///< Range and maxdata config of channels
	unsigned *chanlist;			///< Channel list in comedi's packed format
	size_t chanlist_len;			///< Number of channels for this direction

	char* buffer;
	char* bufptr;
};

struct comedi {
	char *device;
	struct comedi_direction in, out;
	comedi_t *dev;

#if COMEDI_USE_READ
	char *buf;
	char *bufptr;
#else
	char *map;
	size_t bufpos;
	size_t front;
	size_t back;
#endif

};

char * comedi_print(NodeCompat *n);

int comedi_parse(NodeCompat *n, json_t *json);

int comedi_start(NodeCompat *n);

int comedi_stop(NodeCompat *n);

int comedi_poll_fds(NodeCompat *n, int fds[]);

int comedi_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int comedi_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} // namespace node
} // namespace villas
