/** Node type: comedi
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <comedilib.h>

#include <villas/list.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

/* Forward declarations */
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

} /* namespace node */
} /* namespace villas */
