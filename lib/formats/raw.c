/** RAW IO format
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

#include <villas/sample.h>
#include <villas/plugin.h>
#include <villas/utils.h>
#include <villas/formats/raw.h>
#include <villas/compat.h>

/** Convert float to host byte order */
#define SWAP_FLT_TOH(o, n) ({				\
	union { float f; uint32_t i; } x = { .f = n };	\
	x.i = (o) ? be32toh(x.i) : le32toh(x.i); x.f;	\
})

/** Convert double to host byte order */
#define SWAP_DBL_TOH(o, n) ({				\
	union { float f; uint64_t i; } x = { .f = n };	\
	x.i = (o) ? be64toh(x.i) : le64toh(x.i); x.f;	\
})

/** Convert float to big/little endian byte order */
#define SWAP_FLT_TOE(o, n) ({				\
	union { float f; uint32_t i; } x = { .f = n };	\
	x.i = (o) ? htobe32(x.i) : htole32(x.i); x.f;	\
})

/** Convert double to big/little endian byte order */
#define SWAP_DBL_TOE(o, n) ({				\
	union { double f; uint64_t i; } x = { .f = n };	\
	x.i = (o) ? htobe64(x.i) : htole64(x.i); x.f;	\
})

/** Convert integer of varying width to host byte order */
#define SWAP_INT_TOH(o, b, n) (o ? be ## b ## toh(n) : le ## b ## toh(n))

/** Convert integer of varying width to big/little endian byte order */
#define SWAP_INT_TOE(o, b, n) (o ? htobe ## b (n) : htole ## b (n))

int raw_sprint(char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt, int flags)
{

	int i, o = 0;
	size_t nlen;

	int8_t  *i8  = (void *) buf;
	int16_t *i16 = (void *) buf;
	int32_t *i32 = (void *) buf;
	int64_t *i64 = (void *) buf;
	float   *f32 = (void *) buf;
	double  *f64 = (void *) buf;

	int bits = 1 << (flags >> 24);

	for (i = 0; i < cnt; i++) {
		nlen = (smps[i]->length + o + (flags & RAW_FAKE) ? 3 : 0) * (bits / 8);
		if (nlen >= len)
			break;

		/* First three values are sequence, seconds and nano-seconds timestamps */
		if (flags & RAW_FAKE) {
			switch (bits) {
				case 32:
					i32[o++] = SWAP_INT_TOE(flags & RAW_BE_HDR, 32, smps[i]->sequence);
					i32[o++] = SWAP_INT_TOE(flags & RAW_BE_HDR, 32, smps[i]->ts.origin.tv_sec);
					i32[o++] = SWAP_INT_TOE(flags & RAW_BE_HDR, 32, smps[i]->ts.origin.tv_nsec);
					break;
				case 64:
					i64[o++] = SWAP_INT_TOE(flags & RAW_BE_HDR, 64, smps[i]->sequence);
					i64[o++] = SWAP_INT_TOE(flags & RAW_BE_HDR, 64, smps[i]->ts.origin.tv_sec);
					i64[o++] = SWAP_INT_TOE(flags & RAW_BE_HDR, 64, smps[i]->ts.origin.tv_nsec);
					break;
			}
		}

		enum raw_format {
			RAW_FORMAT_INT,
			RAW_FORMAT_FLT
		};

		for (int j = 0; j < smps[i]->length; j++) {
			enum sample_data_format smp_fmt;
			enum raw_format raw_fmt;

			union { double f; uint64_t i; } val;

			if      (flags & RAW_AUTO)
				raw_fmt = smps[i]->format & (1 << i) ? RAW_FORMAT_INT : RAW_FORMAT_FLT;
			else if (flags & RAW_FLT)
				raw_fmt = RAW_FORMAT_FLT;
			else
				raw_fmt = RAW_FORMAT_INT;

			smp_fmt = sample_get_data_format(smps[i], j);

			switch (raw_fmt) {
				case RAW_FORMAT_FLT:
					switch (smp_fmt) {
						case SAMPLE_DATA_FORMAT_INT:   val.f = smps[i]->data[j].i; break;
						case SAMPLE_DATA_FORMAT_FLOAT: val.f = smps[i]->data[j].f; break;
						default:                       val.f = -1; break;
					}

					switch (bits) {
						case 32: f32[o++] = SWAP_FLT_TOE(flags & RAW_BE_FLT, val.f); break;
						case 64: f64[o++] = SWAP_DBL_TOE(flags & RAW_BE_FLT, val.f); break;
					}
					break;

				case RAW_FORMAT_INT:
					switch (smp_fmt) {
						case SAMPLE_DATA_FORMAT_INT:   val.i = smps[i]->data[j].i; break;
						case SAMPLE_DATA_FORMAT_FLOAT: val.i = smps[i]->data[j].f; break;
						default:                       val.i = -1; break;
					}

					switch (bits) {
						case  8: i8 [o++] =                                      val.i;  break;
						case 16: i16[o++] = SWAP_INT_TOE(flags & RAW_BE_INT, 16, val.i); break;
						case 32: i32[o++] = SWAP_INT_TOE(flags & RAW_BE_INT, 32, val.i); break;
						case 64: i64[o++] = SWAP_INT_TOE(flags & RAW_BE_INT, 64, val.i); break;
					}
					break;
			}
		}
	}

	if (wbytes)
		*wbytes = o * (bits / 8);

	return i;
}

int raw_sscan(char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt, int flags)
{
	/* The raw format can not encode multiple samples in one buffer
	 * as there is no support for framing. */
	struct sample *smp = smps[0];

	int8_t  *i8  = (void *) buf;
	int16_t *i16 = (void *) buf;
	int32_t *i32 = (void *) buf;
	int64_t *i64 = (void *) buf;
	float   *f32 = (void *) buf;
	double  *f64 = (void *) buf;

	int off, bits = 1 << (flags >> 24);

	smp->length = len / (bits / 8);

	if (flags & RAW_FAKE) {
		off = 3;

		if (smp->length < off) {
			warn("Received a packet with no fake header. Skipping...");
			return 0;
		}

		smp->length -= off;

		switch (bits) {
			case 32:
				smp->sequence          = SWAP_INT_TOH(flags & RAW_BE_HDR, 32, i32[0]);
				smp->ts.origin.tv_sec  = SWAP_INT_TOH(flags & RAW_BE_HDR, 32, i32[1]);
				smp->ts.origin.tv_nsec = SWAP_INT_TOH(flags & RAW_BE_HDR, 32, i32[2]);
				break;

			case 64:
				smp->sequence          = SWAP_INT_TOH(flags & RAW_BE_HDR, 64, i64[0]);
				smp->ts.origin.tv_sec  = SWAP_INT_TOH(flags & RAW_BE_HDR, 64, i64[1]);
				smp->ts.origin.tv_nsec = SWAP_INT_TOH(flags & RAW_BE_HDR, 64, i64[2]);
				break;
		}

		smp->flags = SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_ORIGIN;
	}
	else {
		off = 0;

		smp->flags = 0;
		smp->sequence = 0;
		smp->ts.origin.tv_sec  = 0;
		smp->ts.origin.tv_nsec = 0;
	}

	if (smp->length > smp->capacity) {
		warn("Received more values than supported: length=%u, capacity=%u", smp->length, smp->capacity);
		smp->length = smp->capacity;
	}

	for (int i = 0; i < smp->length; i++) {
		enum sample_data_format smp_fmt = flags & RAW_FLT ? SAMPLE_DATA_FORMAT_FLOAT
		                                                  : SAMPLE_DATA_FORMAT_INT;

		sample_set_data_format(smp, i, smp_fmt);

		switch (smp_fmt) {
			case SAMPLE_DATA_FORMAT_FLOAT:
				switch (bits) {
					case 32: smp->data[i].f = SWAP_FLT_TOH(flags & RAW_BE_FLT, f32[i+off]); break;
					case 64: smp->data[i].f = SWAP_DBL_TOH(flags & RAW_BE_FLT, f64[i+off]); break;
				}
				break;

			case SAMPLE_DATA_FORMAT_INT:
				switch (bits) {
					case 8:  smp->data[i].i = i8[i]; break;
					case 16: smp->data[i].i = (int16_t) SWAP_INT_TOH(flags & RAW_BE_INT, 16, i16[i+off]); break;
					case 32: smp->data[i].i = (int32_t) SWAP_INT_TOH(flags & RAW_BE_INT, 32, i32[i+off]); break;
					case 64: smp->data[i].i = (int64_t) SWAP_INT_TOH(flags & RAW_BE_INT, 64, i64[i+off]); break;
				}
				break;
		}
	}

	if (rbytes)
		*rbytes = len;

	return 1;
}

#define REGISTER_FORMAT_RAW(i, n, d, f)		\
static struct plugin i = {			\
	.name = n,				\
	.description = d,			\
	.type = PLUGIN_TYPE_FORMAT,			\
	.io = {					\
		.flags = f | format_type_BINARY,	\
		.sprint = raw_sprint,		\
		.sscan  = raw_sscan		\
	}					\
};						\
REGISTER_PLUGIN(& i);

/* Feel free to add additional format identifiers here to suit your needs */
REGISTER_FORMAT_RAW(p_f32,   "raw.flt32",	"Raw single precision floating point", RAW_32 | RAW_FLT)
REGISTER_FORMAT_RAW(p_f64,   "raw.flt64",	"Raw double precision floating point", RAW_64 | RAW_FLT)
REGISTER_FORMAT_RAW(p_i8,    "raw.int8",	"Raw  8 bit, signed integer", RAW_8)
REGISTER_FORMAT_RAW(p_i16be, "raw.int16.be",	"Raw 16 bit, signed integer, big endian byte-order", RAW_16 | RAW_BE)
REGISTER_FORMAT_RAW(p_i16le, "raw.int16.le",	"Raw 16 bit, signed integer, little endian byte-order", RAW_16)
REGISTER_FORMAT_RAW(p_i32be, "raw.int32.be",	"Raw 32 bit, signed integer, big endian byte-order", RAW_32 | RAW_BE)
REGISTER_FORMAT_RAW(p_i32le, "raw.int32.le",	"Raw 32 bit, signed integer, little endian byte-order", RAW_32)
REGISTER_FORMAT_RAW(p_i64be, "raw.int64.be",	"Raw 64 bit, signed integer, bit endian byte-order", RAW_64 | RAW_BE)
REGISTER_FORMAT_RAW(p_i64le, "raw.int64.le",	"Raw 64 bit, signed integer, little endian byte-order", RAW_64)
REGISTER_FORMAT_RAW(p_gtnet, "gtnet",		"RTDS GTNET", RAW_32 | RAW_FLT | RAW_BE)
REGISTER_FORMAT_RAW(p_gtnef, "gtnet.fake",	"RTDS GTNET with fake header", RAW_32 | RAW_FLT | RAW_BE | RAW_FAKE)
