/** The internal datastructure for a sample of simulation data.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#ifndef _SAMPLE_H_
#define _SAMPLE_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <time.h>
#include <sys/types.h>

/* Forward declarations */
struct pool;

/** The length of a sample datastructure with \p values values in bytes. */
#define SAMPLE_LEN(len)	(sizeof(struct sample) + SAMPLE_DATA_LEN(len))

/** The length of a sample data portion of a sample datastructure with \p values values in bytes. */
#define SAMPLE_DATA_LEN(len)	((len) * sizeof(float))

/** The offset to the beginning of the data section. */
#define SAMPLE_DATA_OFFSET(smp)	((char *) (smp) + offsetof(struct sample, data))

/** These flags define the format which is used by sample_fscan() and sample_fprint(). */
enum sample_flags {
	SAMPLE_NANOSECONDS	= 1,
	SAMPLE_OFFSET		= 2,
	SAMPLE_SEQUENCE		= 4,
	SAMPLE_VALUES		= 8,
	SAMPLE_ALL		= 16-1
};

struct sample {
	int sequence;		/**< The sequence number of this sample. */
	int length;		/**< The number of values in sample::values which are valid. */
	int capacity;		/**< The number of values in sample::values for which memory is reserved. */
	
	atomic_int refcnt;	/**< Reference counter. */
	struct pool *pool;	/**< This sample is belong to this memory pool. */

	/** All timestamps are seconds / nano seconds after 1.1.1970 UTC */
	struct {
		struct timespec origin;		/**< The point in time when this data was sampled. */
		struct timespec received;	/**< The point in time when this data was received. */
		struct timespec sent;		/**< The point in time this data was send for the last time. */
	} ts;

	/** The values. */
	union {
		float    f;	/**< Floating point values. */
		uint32_t i;	/**< Integer values. */
	} data[];		/**< Data is in host endianess! */
};

/** Request \p cnt samples from memory pool \p p and initialize them.
 *  This will leave the reference count of the sample to zero.
 *  Use the sample_get() function to increase it. */
int sample_alloc(struct pool *p, struct sample *smps[], int cnt);

/** Release an array of samples back to their pools */
void sample_free(struct sample *smps[], int cnt);

/** Increase reference count of sample */
int sample_get(struct sample *s);

/** Decrease reference count and release memory if last reference was held. */
int sample_put(struct sample *s);

/** Print a sample in human readable form to a file stream.
 *
 * @param buf A character buffer of len bytes.
 * @param len The size of buf.
 * @param m A pointer to the sample.
 * @param flags See sample_flags.
 * @return Number of bytes written to buf.
 */
int sample_print(char *buf, size_t len, struct sample *s, int flags);

/** Read a sample from a character buffer.
 *
 * @param line A string which should be parsed.
 * @param s A pointer to the sample.
 * @param flags et SAMPLE_* flags for each component found in sample if not NULL. See sample_flags.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int sample_scan(const char *line, struct sample *s, int *fl);

/** Print a sample in human readable form to a file stream.
 *
 * @see sample_print()
 * @param f The file handle from fopen() or stdout, stderr.
 * @param s A pointer to the sample.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int sample_fprint(FILE *f, struct sample *s, int flags);

/** Read a sample from a file stream.
 *
 * @see sample_scan()
 * @param f The file handle from fopen() or stdin.
 * @param s A pointer to the sample.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int sample_fscan(FILE *f, struct sample *s, int *flags);

#endif /* _SAMPLE_H_ */