/** Unit tests for formatters.
 *
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

#include <stdio.h>
#include <float.h>
#include <complex>

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <villas/utils.hpp>
#include <villas/timing.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>
#include <villas/pool.hpp>
#include <villas/format.hpp>
#include <villas/log.hpp>

#include "helpers.hpp"

using namespace villas;
using namespace villas::node;

extern void init_memory();

#define NUM_VALUES 10

using string = std::basic_string<char, std::char_traits<char>, criterion::allocator<char>>;

struct Param {
public:
	Param(const char *f, int c, int b) :
		fmt(f), cnt(c), bits(b)
	{}

	string fmt;
	int cnt;
	int bits;
};

void fill_sample_data(SignalList::Ptr signals, struct Sample *smps[], unsigned cnt)
{
	struct timespec delta, now;

	now = time_now();
	delta = time_from_double(50e-6);

	for (unsigned i = 0; i < cnt; i++) {
		struct Sample *smp = smps[i];

		smps[i]->flags = (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA | (int) SampleFlags::HAS_TS_ORIGIN;
		smps[i]->length = signals->size();
		smps[i]->sequence = 235 + i;
		smps[i]->ts.origin = now;
		smps[i]->signals = signals;

		for (size_t j = 0; j < signals->size(); j++) {
			auto sig = signals->getByIndex(j);
			auto *data = &smp->data[j];

			switch (sig->type) {
				case SignalType::BOOLEAN:
					data->b = j * 0.1 + i * 100;
					break;

				case SignalType::COMPLEX: {
					/** @todo Port to proper C++ */
					std::complex<float> z = { j * 0.1f, i * 100.0f };
					memcpy(&data->z, &z, sizeof(data->z));
					break;
				}

				case SignalType::FLOAT:
					data->f = j * 0.1 + i * 100;
					break;

				case SignalType::INTEGER:
					data->i = j + i * 1000;
					break;

				default: { }
			}
		}

		now = time_add(&now, &delta);
	}
}

void cr_assert_eq_sample(struct Sample *a, struct Sample *b, int flags)
{
	cr_assert_eq(a->length, b->length, "a->length=%d, b->length=%d", a->length, b->length);

	if (flags & (int) SampleFlags::HAS_SEQUENCE)
		cr_assert_eq(a->sequence, b->sequence);

	if (flags & (int) SampleFlags::HAS_TS_ORIGIN) {
		cr_assert_eq(a->ts.origin.tv_sec, b->ts.origin.tv_sec);
		cr_assert_eq(a->ts.origin.tv_nsec, b->ts.origin.tv_nsec);
	}

	if (flags & (int) SampleFlags::HAS_DATA) {
		for (unsigned j = 0; j < MIN(a->length, b->length); j++) {
			cr_assert_eq(sample_format(a, j), sample_format(b, j));

			switch (sample_format(b, j)) {
				case SignalType::FLOAT:
					cr_assert_float_eq(a->data[j].f, b->data[j].f, 1e-3, "Sample data mismatch at index %d: %f != %f", j, a->data[j].f, b->data[j].f);
					break;

				case SignalType::INTEGER:
					cr_assert_eq(a->data[j].i, b->data[j].i, "Sample data mismatch at index %d: %lld != %lld", j, a->data[j].i, b->data[j].i);
					break;

				case SignalType::BOOLEAN:
					cr_assert_eq(a->data[j].b, b->data[j].b, "Sample data mismatch at index %d: %s != %s", j, a->data[j].b ? "true" : "false", b->data[j].b ? "true" : "false");
					break;

				case SignalType::COMPLEX: {
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

void cr_assert_eq_sample_raw(struct Sample *a, struct Sample *b, int flags, int bits)
{
	cr_assert_eq(a->length, b->length);

	if (flags & (int) SampleFlags::HAS_SEQUENCE)
		cr_assert_eq(a->sequence, b->sequence);

	if (flags & (int) SampleFlags::HAS_TS_ORIGIN) {
		cr_assert_eq(a->ts.origin.tv_sec, b->ts.origin.tv_sec);
		cr_assert_eq(a->ts.origin.tv_nsec, b->ts.origin.tv_nsec);
	}

	if (flags & (int) SampleFlags::HAS_DATA) {
		for (unsigned j = 0; j < MIN(a->length, b->length); j++) {
			cr_assert_eq(sample_format(a, j), sample_format(b, j));

			switch (sample_format(b, j)) {
				case SignalType::FLOAT:
					if (bits != 8 && bits != 16)
						cr_assert_float_eq(a->data[j].f, b->data[j].f, 1e-3, "Sample data mismatch at index %d: %f != %f", j, a->data[j].f, b->data[j].f);
					break;

				case SignalType::INTEGER:
					cr_assert_eq(a->data[j].i, b->data[j].i, "Sample data mismatch at index %d: %lld != %lld", j, a->data[j].i, b->data[j].i);
					break;

				case SignalType::BOOLEAN:
					cr_assert_eq(a->data[j].b, b->data[j].b, "Sample data mismatch at index %d: %s != %s", j, a->data[j].b ? "true" : "false", b->data[j].b ? "true" : "false");
					break;

				case SignalType::COMPLEX:
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

ParameterizedTestParameters(format, lowlevel)
{
	static criterion::parameters<Param> params;

	params.emplace_back("{ \"type\": \"gtnet\" }",						1, 32);
	params.emplace_back("{ \"type\": \"gtnet\", \"fake\": true }",				1, 32);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 8 }",				1,  8);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 16, \"endianess\": \"big\"    }",	1, 16);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 16, \"endianess\": \"little\" }",	1, 16);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 32, \"endianess\": \"big\"    }",	1, 32);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 32, \"endianess\": \"little\" }",	1, 32);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 64, \"endianess\": \"big\"    }",	1, 64);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 64, \"endianess\": \"little\" }",	1, 64);
	params.emplace_back("{ \"type\": \"villas.human\" }",					10, 0);
	params.emplace_back("{ \"type\": \"villas.binary\" }",					10, 0);
	params.emplace_back("{ \"type\": \"csv\" }",						10, 0);
	params.emplace_back("{ \"type\": \"tsv\" }",						10, 0);
	params.emplace_back("{ \"type\": \"json\" }",						10, 0);
	// params.emplace_back("{ \"type\": \"json.kafka\" }",					10, 0); # broken due to signal names
	// params.emplace_back("{ \"type\": \"json.reserve\" }",				10, 0);
#ifdef PROTOBUF_FOUND
	params.emplace_back("{ \"type\": \"protobuf\" }",					10, 0 );
#endif

	return params;
}

// cppcheck-suppress unknownMacro
ParameterizedTest(Param *p, format, lowlevel, .init = init_memory)
{
	int ret;
	unsigned cnt;
	char buf[8192];
	size_t wbytes, rbytes;

	Logger logger = logging.get("test:format:lowlevel");

	logger->info("Running test for format={}, cnt={}", p->fmt, p->cnt);

	struct Pool pool;
	Format *fmt;
	struct Sample *smps[p->cnt];
	struct Sample *smpt[p->cnt];

	ret = pool_init(&pool, 2 * p->cnt, SAMPLE_LENGTH(NUM_VALUES));
	cr_assert_eq(ret, 0);

	auto signals = std::make_shared<SignalList>(NUM_VALUES, SignalType::FLOAT);

	ret = sample_alloc_many(&pool, smps, p->cnt);
	cr_assert_eq(ret, p->cnt);

	ret = sample_alloc_many(&pool, smpt, p->cnt);
	cr_assert_eq(ret, p->cnt);

	fill_sample_data(signals, smps, p->cnt);

	json_t *json_format = json_loads(p->fmt.c_str(), 0, nullptr);
	cr_assert_not_null(json_format);

	fmt = FormatFactory::make(json_format);
	cr_assert_not_null(fmt, "Failed to create formatter of type '%s'", p->fmt.c_str());

	fmt->start(signals, (int) SampleFlags::HAS_ALL);

	cnt = fmt->sprint(buf, sizeof(buf), &wbytes, smps, p->cnt);
	cr_assert_eq(cnt, p->cnt, "Written only %d of %d samples", cnt, p->cnt);

	cnt = fmt->sscan(buf, wbytes, &rbytes, smpt, p->cnt);
	cr_assert_eq(cnt, p->cnt, "Read only %d of %d samples back", cnt, p->cnt);

	cr_assert_eq(rbytes, wbytes, "rbytes != wbytes: %#zx != %#zx", rbytes, wbytes);

	for (unsigned i = 0; i < cnt; i++) {
		if (p->bits)
			cr_assert_eq_sample_raw(smps[i], smpt[i], fmt->getFlags(), p->bits);
		else
			cr_assert_eq_sample(smps[i], smpt[i], fmt->getFlags());
	}

	sample_free_many(smps, p->cnt);
	sample_free_many(smpt, p->cnt);

	ret = pool_destroy(&pool);
	cr_assert_eq(ret, 0);
}

ParameterizedTestParameters(format, highlevel)
{
	static criterion::parameters<Param> params;

	params.emplace_back("{ \"type\": \"gtnet\" }",						1, 32);
	params.emplace_back("{ \"type\": \"gtnet\", \"fake\": true }",				1, 32);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 8 }",				1,  8);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 16, \"endianess\": \"big\"    }",	1, 16);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 16, \"endianess\": \"little\" }",	1, 16);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 32, \"endianess\": \"big\"    }",	1, 32);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 32, \"endianess\": \"little\" }",	1, 32);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 64, \"endianess\": \"big\"    }",	1, 64);
	params.emplace_back("{ \"type\": \"raw\", \"bits\": 64, \"endianess\": \"little\" }",	1, 64);
	params.emplace_back("{ \"type\": \"villas.human\" }",					10, 0);
	params.emplace_back("{ \"type\": \"villas.binary\" }",					10, 0);
	params.emplace_back("{ \"type\": \"csv\" }",						10, 0);
	params.emplace_back("{ \"type\": \"tsv\" }",						10, 0);
	params.emplace_back("{ \"type\": \"json\" }",						10, 0);
	// params.emplace_back("{ \"type\": \"json.kafka\" }",					10, 0); # broken due to signal names
	// params.emplace_back("{ \"type\": \"json.reserve\" }",				10, 0);
#ifdef PROTOBUF_FOUND
	params.emplace_back("{ \"type\": \"protobuf\" }",					10, 0 );
#endif

	return params;
}

ParameterizedTest(Param *p, format, highlevel, .init = init_memory)
{
	int ret, cnt;
	char *retp;

	Logger logger = logging.get("test:format:highlevel");

	logger->info("Running test for format={}, cnt={}", p->fmt, p->cnt);

	struct Sample *smps[p->cnt];
	struct Sample *smpt[p->cnt];

	struct Pool pool;
	Format *fmt;

	ret = pool_init(&pool, 2 * p->cnt, SAMPLE_LENGTH(NUM_VALUES));
	cr_assert_eq(ret, 0);

	ret = sample_alloc_many(&pool, smps, p->cnt);
	cr_assert_eq(ret, p->cnt);

	ret = sample_alloc_many(&pool, smpt, p->cnt);
	cr_assert_eq(ret, p->cnt);

	auto signals = std::make_shared<SignalList>(NUM_VALUES, SignalType::FLOAT);

	fill_sample_data(signals, smps, p->cnt);

	/* Open a file for testing the formatter */
	char *fn, dir[64];
	strncpy(dir, "/tmp/villas.XXXXXX", sizeof(dir));

	retp = mkdtemp(dir);
	cr_assert_not_null(retp);

	ret = asprintf(&fn, "%s/file", dir);
	cr_assert_gt(ret, 0);

	json_t *json_format = json_loads(p->fmt.c_str(), 0, nullptr);
	cr_assert_not_null(json_format);

	fmt = FormatFactory::make(json_format);
	cr_assert_not_null(fmt, "Failed to create formatter of type '%s'", p->fmt.c_str());

	fmt->start(signals, (int) SampleFlags::HAS_ALL);

	auto *stream = fopen(fn, "w+");
	cr_assert_not_null(stream);

	cnt = fmt->print(stream, smps, p->cnt);
	cr_assert_eq(cnt, p->cnt, "Written only %d of %d samples", cnt, p->cnt);

	ret = fflush(stream);
	cr_assert_eq(ret, 0);

#if 0 /* Show the file contents */
	char cmd[128];
	if (p->fmt == "csv" || p->fmt == "json" || p->fmt == "villas.human")
		snprintf(cmd, sizeof(cmd), "cat %s", fn);
	else
		snprintf(cmd, sizeof(cmd), "hexdump -C %s", fn);
	system(cmd);
#endif

	rewind(stream);

	cnt = fmt->scan(stream, smpt, p->cnt);
	cr_assert_eq(cnt, p->cnt, "Read only %d of %d samples back", cnt, p->cnt);

	for (int i = 0; i < cnt; i++) {
		if (p->bits)
			cr_assert_eq_sample_raw(smps[i], smpt[i], fmt->getFlags(), p->bits);
		else
			cr_assert_eq_sample(smps[i], smpt[i], fmt->getFlags());
	}

	ret = fclose(stream);
	cr_assert_eq(ret, 0);

	delete fmt;

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
