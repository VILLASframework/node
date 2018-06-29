/** Node type: comedi
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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
 * @addtogroup comedi Comedi node type
 * @ingroup node
 * @{
 */

#pragma once

#include <comedilib.h>

#include <villas/node.h>
#include <villas/list.h>
#include <villas/timing.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/** @see node_type::print */
char * comedi_print(struct node *n);

/** @see node_type::parse */
int comedi_parse(struct node *n, json_t *cfg);

/** @see node_type::open */
int comedi_start(struct node *n);

/** @see node_type::close */
int comedi_stop(struct node *n);

/** @see node_type::read */
int comedi_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::write */
int comedi_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */

#ifdef __cplusplus
}
#endif
