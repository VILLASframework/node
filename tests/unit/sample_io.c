/** Unit tests for the sample_io module.
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

#include "utils.h"
#include "timing.h"
#include "sample.h"
#include "sample_io.h"

ParameterizedTestParameters(sample_io, read_write)
{
	static enum sample_io_format formats[] = {
		SAMPLE_IO_FORMAT_VILLAS,
		SAMPLE_IO_FORMAT_JSON,
	};

	return cr_make_param_array(enum sample_io_format, formats, ARRAY_LEN(formats));
}

ParameterizedTest(enum sample_io_format *fmt, sample_io, read_write)
{
	FILE *f;
	int ret;
	struct sample *s, *t;

	/* Prepare a sample with arbitrary data */
	s = malloc(SAMPLE_LEN(16));
	cr_assert_not_null(s);

	t = malloc(SAMPLE_LEN(16));
	cr_assert_not_null(s);

	t->capacity =
	s->capacity = 16;
	s->length = 12;
	s->sequence = 235;
	s->format = 0;
	s->ts.origin = time_now();
	s->ts.received = (struct timespec) { s->ts.origin.tv_sec - 1, s->ts.origin.tv_nsec };

	for (int i = 0; i < s->length; i++)
		s->data[i].f = i * 1.2;

	/* Open a file for IO */
	f = tmpfile();
	cr_assert_not_null(f);

	ret = sample_io_fprint(f, s, *fmt, SAMPLE_IO_ALL);
	cr_assert_eq(ret, 0);

	rewind(f);

	ret = sample_io_fscan(f, t, *fmt, NULL);
	cr_assert_eq(ret, 0);

	cr_assert_eq(s->length, t->length);
	cr_assert_eq(s->sequence, t->sequence);
	cr_assert_eq(s->format, t->format);

	cr_assert_eq(s->ts.origin.tv_sec, t->ts.origin.tv_sec);
	cr_assert_eq(s->ts.origin.tv_nsec, t->ts.origin.tv_nsec);
	cr_assert_eq(s->ts.received.tv_sec, t->ts.received.tv_sec);
	cr_assert_eq(s->ts.received.tv_nsec, t->ts.received.tv_nsec);

	for (int i = 0; i < MIN(s->length, t->length); i++)
		cr_assert_float_eq(s->data[i].f, t->data[i].f, 1e-6);

	fclose(f);

	free(s);
	free(t);
}