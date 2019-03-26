/** The internal datastructure for a sample of simulation data.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <villas/atomic.h>
#include <villas/signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct pool;

/** The length of a sample datastructure with \p values values in bytes. */
#define SAMPLE_LENGTH(len)	(sizeof(struct sample) + SAMPLE_DATA_LENGTH(len))

/** The length of a sample data portion of a sample datastructure with \p values values in bytes. */
#define SAMPLE_DATA_LENGTH(len)	((len) * sizeof(double))

/** The number of values in a sample datastructure. */
#define SAMPLE_NUMBER_OF_VALUES(len)	((len) / sizeof(double))

/** The offset to the beginning of the data section. */
#define SAMPLE_DATA_OFFSET(smp)	((char *) (smp) + offsetof(struct sample, data))

/** Parts of a sample that can be serialized / de-serialized by the IO formats */
enum sample_flags {
	SAMPLE_HAS_TS_ORIGIN	= (1 << 0), /**< Include origin timestamp in output. */
	SAMPLE_HAS_TS_RECEIVED	= (1 << 1), /**< Include receive timestamp in output. */
	SAMPLE_HAS_OFFSET	= (1 << 2), /**< Include offset (received - origin timestamp) in output. */
	SAMPLE_HAS_SEQUENCE	= (1 << 3), /**< Include sequence number in output. */
	SAMPLE_HAS_DATA		= (1 << 4), /**< Include values in output. */
	SAMPLE_HAS_ALL		= (1 << 5) - 1, /**< Enable all output options. */

	SAMPLE_IS_FIRST		= (1 << 16), /**< This sample is the first of a new simulation case */
	SAMPLE_IS_LAST		= (1 << 17) /**< This sample is the last of a running simulation case */
};

struct sample {
	uint64_t sequence;	/**< The sequence number of this sample. */
	unsigned length;	/**< The number of values in sample::values which are valid. */
	unsigned capacity;	/**< The number of values in sample::values for which memory is reserved. */
	int flags;		/**< Flags are used to store binary properties of a sample. */

	struct vlist *signals;	/**< The list of signal descriptors. */

	atomic_int refcnt;	/**< Reference counter. */
	ptrdiff_t pool_off;	/**< This sample belongs to this memory pool (relative pointer). See sample_pool(). */

	/** All timestamps are seconds / nano seconds after 1.1.1970 UTC */
	struct {
		struct timespec origin;		/**< The point in time when this data was sampled. */
		struct timespec received;	/**< The point in time when this data was received. */
	} ts;

	/** The sample signal values.
	 *
	 * This variable length array (VLA) extends over the end of struct sample.
	 * Make sure that pointers to struct sample point to memory blocks of adequate size.
	 * Use the SAMPLE_LENGTH() macro to calculate the required size.
	 *
	 * Metadata describing the details of signal values (such as name, unit, data type and more)
	 * are stored in the struct sample::signals list. Each entry in this list corresponedents
	 * to an entry in the struct sample::data array.
	 */
	union signal_data data[];
};

#define SAMPLE_NON_POOL PTRDIFF_MIN

/** Get the address of the pool to which the sample belongs. */
#define sample_pool(s) ((s)->pool_off == SAMPLE_NON_POOL ? NULL : (struct pool *) ((char *) (s) + (s)->pool_off))

struct sample * sample_alloc(struct pool *p);

struct sample * sample_alloc_mem(int capacity);

struct sample * sample_clone(struct sample *smp);

void sample_free(struct sample *s);

/** Request \p cnt samples from memory pool \p p and initialize them.
 *  The reference count will already be set to 1.
 *  Use the sample_incref() function to increase it. */
int sample_alloc_many(struct pool *p, struct sample *smps[], int cnt);

/** Release an array of samples back to their pools */
void sample_free_many(struct sample *smps[], int cnt);

/** Increase reference count of sample */
int sample_incref(struct sample *s);

/** Decrease reference count and release memory if last reference was held. */
int sample_decref(struct sample *s);

int sample_copy(struct sample *dst, struct sample *src);

/** Dump all details about a sample to debug log */
void sample_dump(struct sample *s);

/** Compare two samples */
int sample_cmp(struct sample *a, struct sample *b, double epsilon, int flags);

int sample_clone_many(struct sample *clones[], struct sample *origs[], int cnt);
int sample_copy_many(struct sample *dsts[], struct sample *srcs[], int cnt);
int sample_incref_many(struct sample *smps[], int cnt);
int sample_decref_many(struct sample *smps[], int cnt);

enum signal_type sample_format(const struct sample *s, unsigned idx);

void sample_data_insert(struct sample *smp, const union signal_data *src, size_t offset, size_t len);

void sample_data_remove(struct sample *smp, size_t offset, size_t len);

#ifdef __cplusplus
}
#endif
