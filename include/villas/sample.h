/** The internal datastructure for a sample of simulation data.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include "atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>

/* Forward declarations */
struct pool;

/** The length of a sample datastructure with \p values values in bytes. */
#define SAMPLE_LEN(len)	(sizeof(struct sample) + SAMPLE_DATA_LEN(len))

/** The length of a sample data portion of a sample datastructure with \p values values in bytes. */
#define SAMPLE_DATA_LEN(len)	((len) * sizeof(double))

/** The offset to the beginning of the data section. */
#define SAMPLE_DATA_OFFSET(smp)	((char *) (smp) + offsetof(struct sample, data))

enum sample_data_format {
	SAMPLE_DATA_FORMAT_FLOAT = 0,
	SAMPLE_DATA_FORMAT_INT   = 1
};

/** Parts of a sample that can be serialized / de-serialized by the IO formats */
enum sample_has {
	SAMPLE_ORIGIN		= (1 << 0), /**< Include nanoseconds in output. */
	SAMPLE_RECEIVED		= (1 << 1), /**< Include nanoseconds in output. */
	SAMPLE_OFFSET		= (1 << 2),
	SAMPLE_SOURCE		= (1 << 3),
	SAMPLE_ID		= (1 << 4),
	SAMPLE_SEQUENCE		= (1 << 5), /**< Include sequence number in output. */
	SAMPLE_VALUES		= (1 << 6), /**< Include values in output. */
	SAMPLE_FORMAT		= (1 << 7),
	SAMPLE_ALL		= (1 << 7) - 1, /**< Enable all output options. */
};

struct sample {
	int sequence; /**< The sequence number of this sample. */
	int length;   /**< The number of values in sample::values which are valid. */
	int capacity; /**< The number of values in sample::values for which memory is reserved. */

	int id;

	atomic_int refcnt;   /**< Reference counter. */
	off_t pool_off;	     /**< This sample belongs to this memory pool (relative pointer). */
	struct node *source; /**< The node from which this sample originates. */

	/** All timestamps are seconds / nano seconds after 1.1.1970 UTC */
	struct {
		struct timespec origin;		/**< The point in time when this data was sampled. */
		struct timespec received;	/**< The point in time when this data was received. */
		struct timespec sent;		/**< The point in time when this data was send for the last time. */
	} ts;

	int has;

	/** A long bitfield indicating the number representation of the first 64 values in sample::data[].
	 *
	 * @see sample_data_format
	 */
	uint64_t format;

	/** The values. */
	union {
		double  f;	/**< Floating point values. */
		int64_t i;	/**< Integer values. */
	} data[];		/**< Data is in host endianess! */
};

#define sample_pool(s) ((struct pool *) ((char *) (s) + (s)->pool_off))

/** Request \p cnt samples from memory pool \p p and initialize them.
 *  The reference count will already be set to 1.
 *  Use the sample_get() function to increase it. */
int sample_alloc(struct pool *p, struct sample *smps[], int cnt);

/** Release an array of samples back to their pools */
void sample_free(struct sample *smps[], int cnt);

/** Increase reference count of sample */
int sample_get(struct sample *s);

/** Decrease reference count and release memory if last reference was held. */
int sample_put(struct sample *s);

int sample_copy(struct sample *dst, struct sample *src);

/** Compare two samples */
int sample_cmp(struct sample *a, struct sample *b, double epsilon, int flags);

int sample_copy_many(struct sample *dsts[], struct sample *srcs[], int cnt);
int sample_get_many(struct sample *smps[], int cnt);
int sample_put_many(struct sample *smps[], int cnt);

/** Get number representation for a single value of a sample. */
int sample_get_data_format(struct sample *s, int idx);

/** Set number representation for a single value of a sample. */
int sample_set_data_format(struct sample *s, int idx, enum sample_data_format fmt);

#ifdef __cplusplus
}
#endif
