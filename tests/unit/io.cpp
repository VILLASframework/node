/** Unit tests for IO formats.
 *
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

#include <stdio.h>
#include <float.h>
#include <complex>

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <villas/utils.h>
#include <villas/timing.h>
#include <villas/sample.h>
#include <villas/signal.h>
#include <villas/plugin.h>
#include <villas/pool.h>
#include <villas/io.h>
#include <villas/log.hpp>

using namespace villas;

extern void init_memory();

#define NUM_VALUES 10

struct param {
	std::basic_string<char, std::char_traits<char>, criterion::allocator<char>> fmt;
	int cnt;
	int bits;
};

static struct param params[] = {
	{ "gtnet",		1, 32 },
	{ "gtnet.fake",		1, 32 },
	{ "raw.8",		1,  8 },
	{ "raw.16.be",		1, 16 },
	{ "raw.16.le",		1, 16 },
	{ "raw.32.be",		1, 32 },
	{ "raw.32.le",		1, 32 },
	{ "raw.64.be",		1, 64 },
	{ "raw.64.le",		1, 64 },
	{ "villas.human",	10, 0 },
	{ "villas.binary",	10, 0 },
	{ "csv",		10, 0 },
	{ "json",		10, 0 },
#ifdef PROTOBUF_FOUND
	{ "protobuf",		10, 0 }
#endif
};

void fill_sample_data(struct vlist *signals, struct sample *smps[], unsigned cnt)
{
	struct timespec delta, now;

	now = time_now();
	delta = time_from_double(50e-6);

	for (unsigned i = 0; i < cnt; i++) {
		struct sample *smp = smps[i];

		smps[i]->flags = SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_DATA | SAMPLE_HAS_TS_ORIGIN;
		smps[i]->length = vlist_length(signals);
		smps[i]->sequence = 235 + i;
		smps[i]->ts.origin = now;
		smps[i]->signals = signals;

		for (size_t j = 0; j < vlist_length(signals); j++) {
			struct signal *sig = (struct signal *) vlist_at(signals, j);
			union signal_data *data = &smp->data[j];

			switch (sig->type) {
				case SIGNAL_TYPE_BOOLEAN:
					data->b = j * 0.1 + i * 100;
					break;

				case SIGNAL_TYPE_COMPLEX: {
					/** @todo: Port to proper C++ */
					std::complex<float> z = { j * 0.1f, i * 100.0f };
					memcpy(&data->z, &z, sizeof(data->z));
					break;
				}

				case SIGNAL_TYPE_FLOAT:
					data->f = j * 0.1 + i * 100;
					break;

				case SIGNAL_TYPE_INTEGER:
					data->i = j + i * 1000;
					break;

				default: { }
			}
		}

		now = time_add(&now, &delta);
	}
}

void cr_assert_eq_sample(struct sample *a, struct sample *b, int flags)
{
	cr_assert_eq(a->length, b->length);

	if (flags & SAMPLE_HAS_SEQUENCE)
		cr_assert_eq(a->sequence, b->sequence);

	if (flags & SAMPLE_HAS_TS_ORIGIN) {
		cr_assert_eq(a->ts.origin.tv_sec, b->ts.origin.tv_sec);
		cr_assert_eq(a->ts.origin.tv_nsec, b->ts.origin.tv_nsec);
	}

	if (flags & SAMPLE_HAS_DATA) {
		for (unsigned j = 0; j < MIN(a->length, b->length); j++) {
			cr_assert_eq(sample_format(a, j), sample_format(b, j));

			switch (sample_format(b, j)) {
				case SIGNAL_TYPE_FLOAT:
					cr_assert_float_eq(a->data[j].f, b->data[j].f, 1e-3, "Sample data mismatch at index %d: %f != %f", j, a->data[j].f, b->data[j].f);
					break;

				case SIGNAL_TYPE_INTEGER:
					cr_assert_eq(a->data[j].i, b->data[j].i, "Sample data mismatch at index %d: %lld != %lld", j, a->data[j].i, b->data[j].i);
					break;

				case SIGNAL_TYPE_BOOLEAN:
					cr_assert_eq(a->data[j].b, b->data[j].b, "Sample data mismatch at index %d: %s != %s", j, a->data[j].b ? "true" : "false", b->data[j].b ? "true" : "false");
					break;

				case SIGNAL_TYPE_COMPLEX: {
					auto ca = * (std::complex<float> *) &a->data[j].z;
					auto cb = * (std::complex<float> *) &b->data[j].z;

					cr_assert_float_eq(std::abs(ca - cb), 0, 1e-6, "Sample data mismatch at index %d: %f+%fi != %f+%fi", j, ca.real(), ca.imag(), cb.real(), cb.imag());
					break;
				}

				default: { }
			}
		}
	}
}

void cr_assert_eq_sample_raw(struct sample *a, struct sample *b, int flags, int bits)
{
	cr_assert_eq(a->length, b->length);

	if (flags & SAMPLE_HAS_SEQUENCE)
		cr_assert_eq(a->sequence, b->sequence);

	if (flags & SAMPLE_HAS_TS_ORIGIN) {
		cr_assert_eq(a->ts.origin.tv_sec, b->ts.origin.tv_sec);
		cr_assert_eq(a->ts.origin.tv_nsec, b->ts.origin.tv_nsec);
	}

	if (flags & SAMPLE_HAS_DATA) {
		for (unsigned j = 0; j < MIN(a->length, b->length); j++) {
			cr_assert_eq(sample_format(a, j), sample_format(b, j));

			switch (sample_format(b, j)) {
				case SIGNAL_TYPE_FLOAT:
					if (bits != 8 && bits != 16)
						cr_assert_float_eq(a->data[j].f, b->data[j].f, 1e-3, "Sample data mismatch at index %d: %f != %f", j, a->data[j].f, b->data[j].f);
					break;

				case SIGNAL_TYPE_INTEGER:
					cr_assert_eq(a->data[j].i, b->data[j].i, "Sample data mismatch at index %d: %lld != %lld", j, a->data[j].i, b->data[j].i);
					break;

				case SIGNAL_TYPE_BOOLEAN:
					cr_assert_eq(a->data[j].b, b->data[j].b, "Sample data mismatch at index %d: %s != %s", j, a->data[j].b ? "true" : "false", b->data[j].b ? "true" : "false");
					break;

				case SIGNAL_TYPE_COMPLEX:
					if (bits != 8 && bits != 16) {
						auto ca = * (std::complex<float> *) &a->data[j].z;
						auto cb = * (std::complex<float> *) &b->data[j].z;

						cr_assert_float_eq(std::abs(ca - cb), 0, 1e-6, "Sample data mismatch at index %d: %f+%fi != %f+%fi", j, ca.real(), ca.imag(), cb.real(), cb.imag());
					}
					break;

				default: { }
			}
		}
	}
}

ParameterizedTestParameters(io, lowlevel)
{
	return criterion_test_params(params);
}

ParameterizedTest(struct param *p, io, lowlevel, .init = init_memory)
{
	int ret;
	unsigned cnt;
	char buf[8192];
	size_t wbytes, rbytes;

	Logger logger = logging.get("test:io:lowlevel");

	logger->info("Running test for format={}, cnt={}", p->fmt, p->cnt);

	struct format_type *f;

	struct pool pool = { .state = STATE_DESTROYED };
	struct io io = { .state = STATE_DESTROYED };
	struct vlist signals = { .state = STATE_DESTROYED };
	struct sample *smps[p->cnt];
	struct sample *smpt[p->cnt];

	ret = pool_init(&pool, 2 * p->cnt, SAMPLE_LENGTH(NUM_VALUES), &memory_hugepage);
	cr_assert_eq(ret, 0);

	vlist_init(&signals);
	signal_list_generate(&signals, NUM_VALUES, SIGNAL_TYPE_FLOAT);

	ret = sample_alloc_many(&pool, smps, p->cnt);
	cr_assert_eq(ret, p->cnt);

	ret = sample_alloc_many(&pool, smpt, p->cnt);
	cr_assert_eq(ret, p->cnt);

	fill_sample_data(&signals, smps, p->cnt);

	f = format_type_lookup(p->fmt.c_str());
	cr_assert_not_null(f, "Format '%s' does not exist", p->fmt.c_str());

	ret = io_init(&io, f, &signals, SAMPLE_HAS_ALL);
	cr_assert_eq(ret, 0);

	ret = io_check(&io);
	cr_assert_eq(ret, 0);

	cnt = io_sprint(&io, buf, sizeof(buf), &wbytes, smps, p->cnt);
	cr_assert_eq(cnt, p->cnt, "Written only %d of %d samples for format %s", cnt, p->cnt, format_type_name(f));

	cnt = io_sscan(&io, buf, wbytes, &rbytes, smpt, p->cnt);
	cr_assert_eq(cnt, p->cnt, "Read only %d of %d samples back for format %s", cnt, p->cnt, format_type_name(f));

	cr_assert_eq(rbytes, wbytes, "rbytes != wbytes: %#zx != %#zx", rbytes, wbytes);

	for (unsigned i = 0; i < cnt; i++) {
		if (p->bits)
			cr_assert_eq_sample_raw(smps[i], smpt[i], f->flags, p->bits);
		else
			cr_assert_eq_sample(smps[i], smpt[i], f->flags);
	}

	sample_free_many(smps, p->cnt);
	sample_free_many(smpt, p->cnt);

	ret = pool_destroy(&pool);
	cr_assert_eq(ret, 0);
}

ParameterizedTestParameters(io, highlevel)
{
	return criterion_test_params(params);
}

ParameterizedTest(struct param *p, io, highlevel, .init = init_memory)
{
	int ret, cnt;
	char *retp;

	Logger logger = logging.get("test:io:highlevel");

	logger->info("Running test for format={}, cnt={}", p->fmt, p->cnt);

	return;

	struct format_type *f;

	struct io io = { .state = STATE_DESTROYED };
	struct pool pool = { .state = STATE_DESTROYED };
	struct vlist signals = { .state = STATE_DESTROYED };

	struct sample *smps[p->cnt];
	struct sample *smpt[p->cnt];

	ret = pool_init(&pool, 2 * p->cnt, SAMPLE_LENGTH(NUM_VALUES), &memory_hugepage);
	cr_assert_eq(ret, 0);

	ret = sample_alloc_many(&pool, smps, p->cnt);
	cr_assert_eq(ret, p->cnt);

	ret = sample_alloc_many(&pool, smpt, p->cnt);
	cr_assert_eq(ret, p->cnt);

	vlist_init(&signals);
	signal_list_generate(&signals, NUM_VALUES, SIGNAL_TYPE_FLOAT);

	fill_sample_data(&signals, smps, p->cnt);

	/* Open a file for IO */
	char *fn, dir[64];
	strncpy(dir, "/tmp/villas.XXXXXX", sizeof(dir));

	retp = mkdtemp(dir);
	cr_assert_not_null(retp);

//	ret = asprintf(&fn, "file://%s/file", dir);
	ret = asprintf(&fn, "%s/file", dir);
	cr_assert_gt(ret, 0);

	f = format_type_lookup(p->fmt.c_str());
	cr_assert_not_null(f, "Format '%s' does not exist", p->fmt.c_str());

	ret = io_init(&io, f, &signals, SAMPLE_HAS_ALL);
	cr_assert_eq(ret, 0);

	ret = io_check(&io);
	cr_assert_eq(ret, 0);

	ret = io_open(&io, fn);
	cr_assert_eq(ret, 0);

	cnt = io_print(&io, smps, p->cnt);
	cr_assert_eq(cnt, p->cnt, "Written only %d of %d samples for format %s", cnt, p->cnt, format_type_name(f));

	ret = io_flush(&io);
	cr_assert_eq(ret, 0);

#if 0 /* Show the file contents */
	char cmd[128];
	if (!strcmp(p->fmt, "csv") || !strcmp(p->fmt, "json") || !strcmp(p->fmt, "villas.human"))
		snprintf(cmd, sizeof(cmd), "cat %s", fn);
	else
		snprintf(cmd, sizeof(cmd), "hexdump -C %s", fn);
	system(cmd);
#endif

	io_rewind(&io);

	if (io.mode == IO_MODE_ADVIO)
		adownload(io.in.stream.adv, 0);

	cnt = io_scan(&io, smpt, p->cnt);
	cr_assert_gt(cnt, 0, "Failed to read samples back: cnt=%d", cnt);

	cr_assert_eq(cnt, p->cnt, "Read only %d of %d samples back", cnt, p->cnt);

	for (int i = 0; i < cnt; i++) {
		if (p->bits)
			cr_assert_eq_sample_raw(smps[i], smpt[i], f->flags, p->bits);
		else
			cr_assert_eq_sample(smps[i], smpt[i], f->flags);
	}

	ret = io_close(&io);
	cr_assert_eq(ret, 0);

	ret = io_destroy(&io);
	cr_assert_eq(ret, 0);

	ret = unlink(fn);
	cr_assert_eq(ret, 0);

	ret = rmdir(dir);
	cr_assert_eq(ret, 0);

	free(fn);

	sample_free_many(smps, p->cnt);
	sample_free_many(smpt, p->cnt);

	ret = pool_destroy(&pool);
	cr_assert_eq(ret, 0);
}
