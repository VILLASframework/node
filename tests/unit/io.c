/** Unit tests for IO formats.
 *
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

#include <stdio.h>
#include <float.h>

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include <criterion/logging.h>

#include "utils.h"
#include "timing.h"
#include "sample.h"
#include "plugin.h"
#include "pool.h"
#include "io.h"
#include "io/raw.h"

#define NUM_SAMPLES 10
#define NUM_VALUES 10

static char formats[][32] = {
#ifdef WITH_HDF5
	"hdf5",
#endif
	"raw-int8",
	"raw-int16-be",
	"raw-int16-le",
	"raw-int32-be",
	"raw-int32-le",
	"raw-int64-be",
	"raw-int64-le",
	"raw-flt32",
	"raw-flt64",
	"villas-human",
	"villas-binary",
	"csv",
	"json",
	"gtnet",
	"gtnet-fake"
};

void generate_samples(struct pool *p, struct sample *smps[], struct sample *smpt[], unsigned cnt, unsigned values)
{
	int ret;

	/* Prepare a sample with arbitrary data */
	ret = sample_alloc(p, smps, cnt);
	cr_assert_eq(ret, NUM_SAMPLES);

	ret = sample_alloc(p, smpt, cnt);
	cr_assert_eq(ret, cnt);

	for (int i = 0; i < cnt; i++) {
		smpt[i]->capacity = values;

		smps[i]->length = values;
		smps[i]->sequence = 235 + i;
		smps[i]->format = 0; /* all float */
		smps[i]->ts.origin = time_now();

		for (int j = 0; j < smps[i]->length; j++)
			smps[i]->data[j].f = j * 0.1 + i * 100;
			//smps[i]->data[j].i = -500  + j*100;
	}
}

void cr_assert_eq_samples(struct io_format *f, struct sample *smps[], struct sample *smpt[], unsigned cnt)
{
	/* The RAW format has certain limitations:
	 *  - limited accuracy if smaller datatypes are used
	 *  - no support for vectors / framing
	 *
	 * We need to take these limitations into account before comparing.
	 */
	if (f->sscan == raw_sscan) {
		cr_assert_eq(cnt, 1);
		cr_assert_eq(smpt[0]->length, smpt[0]->capacity, "Expected values: %d, Received values: %d", smpt[0]->capacity, smpt[0]->length);

		if (f->flags & RAW_FAKE) {

		}
		else {
			smpt[0]->sequence = smps[0]->sequence;
			smpt[0]->ts.origin = smps[0]->ts.origin;
		}

		int bits = 1 << (f->flags >> 24);
		for (int j = 0; j < smpt[0]->length; j++) {
			if (f->flags & RAW_FLT) {
				switch (bits) {
					case  32: smps[0]->data[j].f = (float)  smps[0]->data[j].f; break;
					case  64: smps[0]->data[j].f = (double) smps[0]->data[j].f; break;
				}
			}
			else {
				switch (bits) {
					case   8: smps[0]->data[j].i = (  int8_t) smps[0]->data[j].i; break;
					case  16: smps[0]->data[j].i = ( int16_t) smps[0]->data[j].i; break;
					case  32: smps[0]->data[j].i = ( int32_t) smps[0]->data[j].i; break;
					case  64: smps[0]->data[j].i = ( int64_t) smps[0]->data[j].i; break;
				}
			}
		}
	}
	else
		cr_assert_eq(cnt, NUM_SAMPLES, "Read only %d of %d samples back", cnt, NUM_SAMPLES);

	for (int i = 0; i < cnt; i++) {
		cr_assert_eq(smps[i]->length, smpt[i]->length);
		cr_assert_eq(smps[i]->sequence, smpt[i]->sequence);

		cr_assert_eq(smps[i]->ts.origin.tv_sec, smpt[i]->ts.origin.tv_sec);
		cr_assert_eq(smps[i]->ts.origin.tv_nsec, smpt[i]->ts.origin.tv_nsec);

		for (int j = 0; j < MIN(smps[i]->length, smpt[i]->length); j++) {
			switch (sample_get_data_format(smpt[i], j)) {
				case SAMPLE_DATA_FORMAT_FLOAT:
					cr_assert_float_eq(smps[i]->data[j].f, smpt[i]->data[j].f, 1e-3, "Sample data mismatch at index %d: %f != %f", j, smps[i]->data[j].f, smpt[i]->data[j].f);
					break;
				case SAMPLE_DATA_FORMAT_INT:
					cr_assert_eq(smps[i]->data[j].i, smpt[i]->data[j].i);
					break;
			}
		}
	}
}

ParameterizedTestParameters(io, lowlevel)
{
	return cr_make_param_array(char[32], formats, ARRAY_LEN(formats));
}

ParameterizedTest(char *fmt, io, lowlevel)
{
	int ret;
	char buf[8192];
	size_t wbytes, rbytes;

	struct io_format *f;

	struct pool p = { .state = STATE_DESTROYED };
	struct sample *smps[NUM_SAMPLES];
	struct sample *smpt[NUM_SAMPLES];

	ret = pool_init(&p, 2 * NUM_SAMPLES, SAMPLE_LEN(NUM_VALUES), &memtype_hugepage);
	cr_assert_eq(ret, 0);

	generate_samples(&p, smps, smpt, NUM_SAMPLES, NUM_VALUES);

	f = io_format_lookup(fmt);
	cr_assert_not_null(f, "Format '%s' does not exist", fmt);

	ret = io_format_sprint(f, buf, sizeof(buf), &wbytes, smps, NUM_SAMPLES, SAMPLE_ALL);
	cr_assert_eq(ret, NUM_SAMPLES);

	ret = io_format_sscan(f, buf, wbytes, &rbytes, smpt, NUM_SAMPLES, 0);
	cr_assert_eq(rbytes, wbytes);

	cr_assert_eq_samples(f, smps, smpt, ret);

	sample_free(smps, NUM_SAMPLES);
	sample_free(smpt, NUM_SAMPLES);

	ret = pool_destroy(&p);
	cr_assert_eq(ret, 0);
}

ParameterizedTestParameters(io, highlevel)
{
	return cr_make_param_array(char[32], formats, ARRAY_LEN(formats));
}

ParameterizedTest(char *fmt, io, highlevel)
{
	int ret, cnt;

	struct io io;
	struct io_format *f;

	struct pool p = { .state = STATE_DESTROYED };
	struct sample *smps[NUM_SAMPLES];
	struct sample *smpt[NUM_SAMPLES];

	ret = pool_init(&p, 2 * NUM_SAMPLES, SAMPLE_LEN(NUM_VALUES), &memtype_hugepage);
	cr_assert_eq(ret, 0);

	generate_samples(&p, smps, smpt, NUM_SAMPLES, NUM_VALUES);

	/* Open a file for IO */
	char *fn, dir[64];
	strncpy(dir, "/tmp/villas.XXXXXX", sizeof(dir));

	mkdtemp(dir);
//	ret = asprintf(&fn, "file://%s/file", dir);
	ret = asprintf(&fn, "%s/file", dir);
	cr_assert_gt(ret, 0);

	f = io_format_lookup(fmt);
	cr_assert_not_null(f, "Format '%s' does not exist", fmt);

	ret = io_init(&io, f, SAMPLE_ALL);
	cr_assert_eq(ret, 0);

	ret = io_open(&io, fn);
	cr_assert_eq(ret, 0);

	ret = io_print(&io, smps, NUM_SAMPLES);
	cr_assert_eq(ret, NUM_SAMPLES);

	ret = io_flush(&io);
	cr_assert_eq(ret, 0);

#if 0 /* Show the file contents */
	char cmd[128];
	if (!strcmp(fmt, "json") || !strcmp(fmt, "villas-human"))
		snprintf(cmd, sizeof(cmd), "cat %s", fn);
	else
		snprintf(cmd, sizeof(cmd), "hexdump -C %s", fn);
	system(cmd);
#endif

	if (io.mode == IO_MODE_ADVIO)
		adownload(io.advio.input, 0);

	io_rewind(&io);

	cnt = io_scan(&io, smpt, NUM_SAMPLES);
	cr_assert_gt(cnt, 0, "Failed to read samples back");

	cr_assert_eq_samples(f, smps, smpt, cnt);

	ret = io_close(&io);
	cr_assert_eq(ret, 0);

	ret = io_destroy(&io);
	cr_assert_eq(ret, 0);

	ret = unlink(fn);
	cr_assert_eq(ret, 0);

	ret = rmdir(dir);
	cr_assert_eq(ret, 0);

	free(fn);

	sample_free(smps, NUM_SAMPLES);
	sample_free(smpt, NUM_SAMPLES);

	ret = pool_destroy(&p);
	cr_assert_eq(ret, 0);
}
