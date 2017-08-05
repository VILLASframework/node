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

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include <criterion/logging.h>

#include "utils.h"
#include "timing.h"
#include "sample.h"
#include "plugin.h"
#include "pool.h"
#include "io.h"

#define NUM_SAMPLES 10
#define NUM_VALUES 10

void cr_assert_eq_sample(struct sample *s, struct sample *t)
{
	cr_assert_eq(s->length, t->length);
	cr_assert_eq(s->sequence, t->sequence);
	cr_assert_eq(s->format, t->format);

	cr_assert_eq(s->ts.origin.tv_sec, t->ts.origin.tv_sec);
	cr_assert_eq(s->ts.origin.tv_nsec, t->ts.origin.tv_nsec);
	cr_assert_eq(s->ts.received.tv_sec, t->ts.received.tv_sec);
	cr_assert_eq(s->ts.received.tv_nsec, t->ts.received.tv_nsec);

	for (int i = 0; i < MIN(s->length, t->length); i++)
		cr_assert_float_eq(s->data[i].f, t->data[i].f, 1e-6);
}

ParameterizedTestParameters(io, highlevel)
{
	static char formats[][32] = {
		"villas",
		"json",
		"csv"
	};

	return cr_make_param_array(char[32], formats, ARRAY_LEN(formats));
}

ParameterizedTest(char *fmt, io, highlevel)
{
	int ret;
	char filename[64];

	struct io io;
	struct pool p = { .state = STATE_DESTROYED };
	struct sample *smps[NUM_SAMPLES];
	struct sample *smpt[NUM_SAMPLES];

	ret = pool_init(&p, 2 * NUM_SAMPLES, SAMPLE_LEN(NUM_VALUES), &memtype_hugepage);
	cr_assert_eq(ret, 0);

	/* Prepare a sample with arbitrary data */
	ret = sample_alloc(&p, smps, NUM_SAMPLES);
	cr_assert_eq(ret, NUM_SAMPLES);

	ret = sample_alloc(&p, smpt, NUM_SAMPLES);
	cr_assert_eq(ret, NUM_SAMPLES);

	for (int i = 0; i < NUM_SAMPLES; i++) {
		smpt[i]->capacity =
		smps[i]->capacity = NUM_VALUES;
		smps[i]->length = NUM_VALUES;
		smps[i]->sequence = 235 + i;
		smps[i]->format = 0; /* all float */
		smps[i]->ts.origin = time_now();
		smps[i]->ts.received = (struct timespec) {
			.tv_sec  = smps[i]->ts.origin.tv_sec - 1,
			.tv_nsec = smps[i]->ts.origin.tv_nsec
		};

		for (int j = 0; j < smps[i]->length; j++)
			smps[i]->data[j].f = j * 1.2 + i * 100;
	}

	/* Open a file for IO */
	strncpy(filename, "/tmp/villas-unit-test.XXXXXX", sizeof(filename));
	mktemp(filename);

	printf("Writing to file: %s\n", filename);

	struct plugin *pl;

	pl = plugin_lookup(PLUGIN_TYPE_FORMAT, fmt);
	cr_assert_not_null(pl, "Format '%s' does not exist", fmt);

	ret = io_init(&io, &pl->io, IO_FORMAT_ALL);
	cr_assert_eq(ret, 0);

	ret = io_open(&io, filename, "w+");
	cr_assert_eq(ret, 0);

	ret = io_print(&io, smps, NUM_SAMPLES);
	cr_assert_eq(ret, NUM_SAMPLES);

	io_rewind(&io);
	io_flush(&io);

	char cmd[128];
	snprintf(cmd, sizeof(cmd), "cat %s", filename);
	system(cmd);

	ret = io_scan(&io, smpt, NUM_SAMPLES);
	cr_assert_eq(ret, NUM_SAMPLES, "Read only %d of %d samples back", ret, NUM_SAMPLES);

	for (int i = 0; i < 0; i++)
		cr_assert_eq_sample(smps[i], smpt[i]);

	ret = io_close(&io);
	cr_assert_eq(ret, 0);

	ret = io_destroy(&io);
	cr_assert_eq(ret, 0);

	ret = unlink(filename);
	cr_assert_eq(ret, 0);

	sample_free(smps, NUM_SAMPLES);
	sample_free(smpt, NUM_SAMPLES);

	ret = pool_destroy(&p);
	cr_assert_eq(ret, 0);
}
