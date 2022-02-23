/** RAW IO format
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/sample.hpp>
#include <villas/utils.hpp>
#include <villas/formats/raw.hpp>
#include <villas/compat.hpp>
#include <villas/exceptions.hpp>

typedef float flt32_t;
typedef double flt64_t;
typedef long double flt128_t; /** @todo check */

using namespace villas;
using namespace villas::node;

/** Convert double to host byte order */
#define SWAP_FLOAT_XTOH(o, b, n) ({					\
	union { flt ## b ## _t f; uint ## b ## _t i; } x = { .f = n };	\
	x.i = (o) ? be ## b ## toh(x.i) : le ## b ## toh(x.i);		\
	x.f;								\
})

/** Convert double to big/little endian byte order */
#define SWAP_FLOAT_HTOX(o, b, n) ({					\
	union { flt ## b ## _t f; uint ## b ## _t i; } x = { .f = n };	\
	x.i = (o) ? htobe ## b (x.i) : htole ## b (x.i);		\
	x.f;								\
})

/** Convert integer of varying width to host byte order */
#define SWAP_INT_XTOH(o, b, n) (o ? be ## b ## toh(n) : le ## b ## toh(n))

/** Convert integer of varying width to big/little endian byte order */
#define SWAP_INT_HTOX(o, b, n) (o ? htobe ## b (n) : htole ## b (n))

int RawFormat::sprint(char *buf, size_t len, size_t *wbytes, const struct Sample * const smps[], unsigned cnt)
{
	int o = 0;
	size_t nlen;

	void *vbuf = (char *) buf;  /* Avoid warning about invalid pointer cast */

	int8_t     *i8  =  (int8_t *) vbuf;
	int16_t    *i16 =  (int16_t *) vbuf;
	int32_t    *i32 =  (int32_t *) vbuf;
	int64_t    *i64 =  (int64_t *) vbuf;
	float      *f32 =  (float *) vbuf;
	double     *f64 =  (double *) vbuf;
#ifdef HAS_128BIT
	__int128   *i128 = (__int128 *) vbuf;
	__float128 *f128 = (__float128 *) vbuf;
#endif

	for (unsigned i = 0; i < cnt; i++) {
		const struct Sample *smp = smps[i];

		/* First three values are sequence, seconds and nano-seconds timestamps
		*
		* These fields are always encoded as integers!
		*/
		if (fake) {
			/* Check length */
			nlen = (o + 3) * (bits / 8);
			if (nlen >= len)
				goto out;

			switch (bits) {
				case 8:
					i8[o++] = smp->sequence;
					i8[o++] = smp->ts.origin.tv_sec;
					i8[o++] = smp->ts.origin.tv_nsec;
					break;

				case 16:
					i16[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 16, smp->sequence);
					i16[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 16, smp->ts.origin.tv_sec);
					i16[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 16, smp->ts.origin.tv_nsec);
					break;

				case 32:
					i32[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 32, smp->sequence);
					i32[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 32, smp->ts.origin.tv_sec);
					i32[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 32, smp->ts.origin.tv_nsec);
					break;

				case 64:
					i64[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 64, smp->sequence);
					i64[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 64, smp->ts.origin.tv_sec);
					i64[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 64, smp->ts.origin.tv_nsec);
					break;

#ifdef HAS_128BIT
				case 128:
					i128[o++] = SWAP_INT_TO_LE(endianess == Endianess::BIG, 128, smp->sequence);
					i128[o++] = SWAP_INT_TO_LE(endianess == Endianess::BIG, 128, smp->ts.origin.tv_sec);
					i128[o++] = SWAP_INT_TO_LE(endianess == Endianess::BIG, 128, smp->ts.origin.tv_nsec);
					break;
#endif
			}
		}

		for (unsigned j = 0; j < smp->length; j++) {
			enum SignalType fmt = sample_format(smp, j);
			const union SignalData *data = &smp->data[j];

			/* Check length */
			nlen = (o + (fmt == SignalType::COMPLEX ? 2 : 1)) * (bits / 8);
			if (nlen >= len)
				goto out;

			switch (fmt) {
				case SignalType::FLOAT:
					switch (bits) {
						case 8:
							i8 [o++] = -1;
							break; /* Not supported */

						case 16:
							i16[o++] = -1;
							break; /* Not supported */

						case 32:
							f32[o++] = SWAP_FLOAT_HTOX(endianess == Endianess::BIG,  32, (float) data->f);
							break;

						case 64:
							f64[o++] = SWAP_FLOAT_HTOX(endianess == Endianess::BIG,  64, data->f);
							break;

#ifdef HAS_128BIT
						case 128: f128[o++] = SWAP_FLOAT_HTOX(endianess == Endianess::BIG, 128, data->f); break;
#endif
					}
					break;

				case SignalType::INTEGER:
					switch (bits) {
						case 8:
							i8 [o++] = data->i;
							break;

						case 16:
							i16[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 16,  data->i);
							break;

						case 32:
							i32[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 32,  data->i);
							break;

						case 64:
							i64[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 64,  data->i);
							break;

#ifdef HAS_128BIT
						case 128:
							i128[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 128, data->i);
							break;
#endif
					}
					break;

				case SignalType::BOOLEAN:
					switch (bits) {
						case 8:
							i8 [o++] = data->b ? 1 : 0;
							break;

						case 16:
							i16[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 16,  data->b ? 1 : 0);
							break;

						case 32:
							i32[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 32,  data->b ? 1 : 0);
							break;

						case 64:
							i64[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 64,  data->b ? 1 : 0);
							break;

#ifdef HAS_128BIT
						case 128:
							i128[o++] = SWAP_INT_HTOX(endianess == Endianess::BIG, 128, data->b ? 1 : 0);
							break;
#endif
					}
					break;

				case SignalType::COMPLEX:
					switch (bits) {
						case  8:
							i8 [o++]  = -1; /* Not supported */
							i8 [o++]  = -1;
							break;

						case 16:
							i16[o++]  = -1; /* Not supported */
							i16[o++]  = -1;
							break;

						case 32:
							f32[o++]  = SWAP_FLOAT_HTOX(endianess == Endianess::BIG,  32, (float) std::real(data->z));
							f32[o++]  = SWAP_FLOAT_HTOX(endianess == Endianess::BIG,  32, (float) std::imag(data->z));
							break;

						case 64:
							f64[o++]  = SWAP_FLOAT_HTOX(endianess == Endianess::BIG,  64, std::real(data->z));
							f64[o++]  = SWAP_FLOAT_HTOX(endianess == Endianess::BIG,  64, std::imag(data->z));
							break;
#ifdef HAS_128BIT
						case 128:
							f128[o++] = SWAP_FLOAT_HTOX(endianess == Endianess::BIG, 128, std::real(data->z));
							f128[o++] = SWAP_FLOAT_HTOX(endianess == Endianess::BIG, 128, std::imag(data->z));
							break;
#endif
					}
					break;

				case SignalType::INVALID:
					return -1;
			}
		}
	}

out:	if (wbytes)
		*wbytes = o * (bits / 8);

	return cnt;
}

int RawFormat::sscan(const char *buf, size_t len, size_t *rbytes, struct Sample * const smps[], unsigned cnt)
{
	void *vbuf = (void *) buf; /* Avoid warning about invalid pointer cast */

	int8_t     *i8  =  (int8_t *)  vbuf;
	int16_t    *i16 =  (int16_t *) vbuf;
	int32_t    *i32 =  (int32_t *) vbuf;
	int64_t    *i64 =  (int64_t *) vbuf;
	float      *f32 =  (float *) vbuf;
	double     *f64 =  (double *) vbuf;
#ifdef HAS_128BIT
	__int128   *i128 = (__int128 *) vbuf;
	__float128 *f128 = (__float128 *) vbuf;
#endif

	/* The raw format can not encode multiple samples in one buffer
	 * as there is no support for framing. */
	struct Sample *smp = smps[0];

	int o = 0;
	int nlen = len / (bits / 8);

	if (cnt > 1)
		return -1;

	if (len % (bits / 8))
		return -1; /* Invalid RAW Payload length */

	if (fake) {
		if (nlen < o + 3)
			return -1; /* Received a packet with no fake header. Skipping... */

		switch (bits) {
			case 8:
				smp->sequence          = i8[o++];
				smp->ts.origin.tv_sec  = i8[o++];
				smp->ts.origin.tv_nsec = i8[o++];
				break;

			case 16:
				smp->sequence          = SWAP_INT_XTOH(endianess == Endianess::BIG, 16, i16[o++]);
				smp->ts.origin.tv_sec  = SWAP_INT_XTOH(endianess == Endianess::BIG, 16, i16[o++]);
				smp->ts.origin.tv_nsec = SWAP_INT_XTOH(endianess == Endianess::BIG, 16, i16[o++]);
				break;

			case 32:
				smp->sequence          = SWAP_INT_XTOH(endianess == Endianess::BIG, 32, i32[o++]);
				smp->ts.origin.tv_sec  = SWAP_INT_XTOH(endianess == Endianess::BIG, 32, i32[o++]);
				smp->ts.origin.tv_nsec = SWAP_INT_XTOH(endianess == Endianess::BIG, 32, i32[o++]);
				break;

			case 64:
				smp->sequence          = SWAP_INT_XTOH(endianess == Endianess::BIG, 64, i64[o++]);
				smp->ts.origin.tv_sec  = SWAP_INT_XTOH(endianess == Endianess::BIG, 64, i64[o++]);
				smp->ts.origin.tv_nsec = SWAP_INT_XTOH(endianess == Endianess::BIG, 64, i64[o++]);
				break;

#ifdef HAS_128BIT
			case 128:
				smp->sequence          = SWAP_INT_XTOH(endianess == Endianess::BIG, 128, i128[o++]);
				smp->ts.origin.tv_sec  = SWAP_INT_XTOH(endianess == Endianess::BIG, 128, i128[o++]);
				smp->ts.origin.tv_nsec = SWAP_INT_XTOH(endianess == Endianess::BIG, 128, i128[o++]);
				break;
#endif
		}

		smp->flags = (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_TS_ORIGIN;
	}
	else {
		smp->flags = 0;
		smp->sequence = 0;
		smp->ts.origin.tv_sec  = 0;
		smp->ts.origin.tv_nsec = 0;
	}

	smp->signals = signals;

	unsigned i;
	for (i = 0; i < smp->capacity && o < nlen; i++) {
		enum SignalType fmt = sample_format(smp, i);
		union SignalData *data = &smp->data[i];

		switch (fmt) {
			case SignalType::FLOAT:
				switch (bits) {
					case 8:   data->f = -1; o++; break; /* Not supported */
					case 16:  data->f = -1; o++; break; /* Not supported */

					case 32:  data->f = SWAP_FLOAT_XTOH(endianess == Endianess::BIG, 32, f32[o++]); break;
					case 64:  data->f = SWAP_FLOAT_XTOH(endianess == Endianess::BIG, 64, f64[o++]); break;
#ifdef HAS_128BIT
					case 128: data->f = SWAP_FLOAT_XTOH(endianess == Endianess::BIG, 128, f128[o++]); break;
#endif
				}
				break;

			case SignalType::INTEGER:
				switch (bits) {
					case 8:   data->i = (int8_t)                                                    i8[o++];  break;
					case 16:  data->i = (int16_t)  SWAP_INT_XTOH(endianess == Endianess::BIG,  16,  i16[o++]); break;
					case 32:  data->i = (int32_t)  SWAP_INT_XTOH(endianess == Endianess::BIG,  32,  i32[o++]); break;
					case 64:  data->i = (int64_t)  SWAP_INT_XTOH(endianess == Endianess::BIG,  64,  i64[o++]); break;
#ifdef HAS_128BIT
					case 128: data->i = (__int128) SWAP_INT_XTOH(endianess == Endianess::BIG, 128, i128[o++]); break;
#endif
				}
				break;

			case SignalType::BOOLEAN:
				switch (bits) {
					case 8:   data->b = (bool)                                                  i8[o++];  break;
					case 16:  data->b = (bool) SWAP_INT_XTOH(endianess == Endianess::BIG,  16,  i16[o++]); break;
					case 32:  data->b = (bool) SWAP_INT_XTOH(endianess == Endianess::BIG,  32,  i32[o++]); break;
					case 64:  data->b = (bool) SWAP_INT_XTOH(endianess == Endianess::BIG,  64,  i64[o++]); break;
#ifdef HAS_128BIT
					case 128: data->b = (bool) SWAP_INT_XTOH(endianess == Endianess::BIG, 128, i128[o++]); break;
#endif
				}
				break;

			case SignalType::COMPLEX:
				switch (bits) {
					case 8:  data->z = std::complex<float>(-1, -1); o += 2; break; /* Not supported */
					case 16: data->z = std::complex<float>(-1, -1); o += 2; break; /* Not supported */

					case 32: data->z = std::complex<float>(
								SWAP_FLOAT_XTOH(endianess == Endianess::BIG, 32, f32[o++]),
								SWAP_FLOAT_XTOH(endianess == Endianess::BIG, 32, f32[o++]));

						break;

					case 64: data->z = std::complex<float>(
								SWAP_FLOAT_XTOH(endianess == Endianess::BIG, 64, f64[o++]),
								SWAP_FLOAT_XTOH(endianess == Endianess::BIG, 64, f64[o++]));
						break;

#if HAS_128BIT
					case 128: data->z = std::complex<float>(
								SWAP_FLOAT_XTOH(endianess == Endianess::BIG, 128, f128[o++]),
								SWAP_FLOAT_XTOH(endianess == Endianess::BIG, 128, f128[o++]));
						break;
#endif
				}
				break;

			case SignalType::INVALID:
				return -1; /* Unsupported format in RAW payload */
		}
	}

	smp->length = i;

	if (smp->length > smp->capacity)
		smp->length = smp->capacity; /* Received more values than supported */

	if (rbytes)
		*rbytes = o * (bits / 8);

	return 1;
}

void RawFormat::parse(json_t *json)
{
	int ret;
	json_error_t err;

	int fake_tmp = 0;
	const char *end = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: b, s?: i }",
		"endianess", &end,
		"fake", &fake_tmp,
		"bits", &bits
	);
	if (ret)
		throw ConfigError(json, err, "node-config-format-raw", "Failed to parse format configuration");

	if (bits % 8 != 0 || bits > 128 || bits <= 0)
		throw ConfigError(json, "node-config-format-raw-bits", "Failed to parse format configuration");

	if (end) {
		if (bits <= 8)
			throw ConfigError(json, "node-config-format-raw-endianess", "An endianess settings must only provided for bits > 8");

		if      (!strcmp(end, "little"))
			endianess = Endianess::LITTLE;
		else if (!strcmp(end, "big"))
			endianess = Endianess::BIG;
		else
			throw ConfigError(json, "node-config-format-raw-endianess", "Endianess must be either 'big' or 'little'");
	}

	fake = fake_tmp;
	if (fake)
		flags |= (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_TS_ORIGIN;
	else
		flags &= ~((int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_TS_ORIGIN);

	BinaryFormat::parse(json);
}

static char n1[] = "raw";
static char d1[] = "Raw binary data";
static FormatPlugin<RawFormat, n1, d1, (int) SampleFlags::HAS_DATA> p1;

static char n2[] = "gtnet";
static char d2[] = "RTDS GTNET";
static FormatPlugin<GtnetRawFormat, n2, d2, (int) SampleFlags::HAS_DATA> p2;
