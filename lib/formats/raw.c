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
#include <villas/io.h>
#include <villas/formats/raw.h>
#include <villas/compat.h>

typedef float flt32_t;
typedef double flt64_t;
typedef long double flt128_t; /** @todo: check */

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

int raw_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	int o = 0;
	size_t nlen;

	int8_t     *i8  =  (void *) buf;
	int16_t    *i16 =  (void *) buf;
	int32_t    *i32 =  (void *) buf;
	int64_t    *i64 =  (void *) buf;
	float      *f32 =  (void *) buf;
	double     *f64 =  (void *) buf;
#ifdef HAS_128BIT
	__int128   *i128 = (void *) buf;
	__float128 *f128 = (void *) buf;
#endif

	int bits = 1 << (io->flags >> 24);

	/* The raw format can not encode multiple samples in one buffer
	 * as there is no support for framing. */
	struct sample *smp = smps[0];

	if (cnt > 1)
		return -1;

	/* First three values are sequence, seconds and nano-seconds timestamps
	 *
	 * These fields are always encoded as integers!
	 */
	if (io->flags & RAW_FAKE_HEADER) {
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
				i16[o++] = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 16, smp->sequence);
				i16[o++] = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 16, smp->ts.origin.tv_sec);
				i16[o++] = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 16, smp->ts.origin.tv_nsec);
				break;

			case 32:
				i32[o++] = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 32, smp->sequence);
				i32[o++] = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 32, smp->ts.origin.tv_sec);
				i32[o++] = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 32, smp->ts.origin.tv_nsec);
				break;

			case 64:
				i64[o++] = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 64, smp->sequence);
				i64[o++] = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 64, smp->ts.origin.tv_sec);
				i64[o++] = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 64, smp->ts.origin.tv_nsec);
				break;

#ifdef HAS_128BIT
			case 128:
				i128[o++] = SWAP_INT_TO_LE(io->flags & RAW_BIG_ENDIAN, 128, smp->sequence);
				i128[o++] = SWAP_INT_TO_LE(io->flags & RAW_BIG_ENDIAN, 128, smp->ts.origin.tv_sec);
				i128[o++] = SWAP_INT_TO_LE(io->flags & RAW_BIG_ENDIAN, 128, smp->ts.origin.tv_nsec);
				break;
#endif
		}
	}

	for (int j = 0; j < smp->length; j++) {
		enum signal_type fmt = sample_format(smp, j);
		union signal_data *data = &smp->data[j];

		/* Check length */
		nlen = (o + fmt == SIGNAL_TYPE_COMPLEX ? 2 : 1) * (bits / 8);
		if (nlen >= len)
			goto out;

		switch (fmt) {
			case SIGNAL_TYPE_FLOAT:
				switch (bits) {
					case  8:  i8 [o++]  = -1; break; /* Not supported */
					case 16:  i16[o++]  = -1; break; /* Not supported */

					case 32:  f32[o++]  = SWAP_FLOAT_HTOX(io->flags & RAW_BIG_ENDIAN,  32, data->f); break;
					case 64:  f64[o++]  = SWAP_FLOAT_HTOX(io->flags & RAW_BIG_ENDIAN,  64, data->f); break;
#ifdef HAS_128BIT
					case 128: f128[o++] = SWAP_FLOAT_HTOX(io->flags & RAW_BIG_ENDIAN, 128, data->f); break;
#endif
				}
				break;

			case SIGNAL_TYPE_INTEGER:
				switch (bits) {
					case  8:  i8 [o++]  =                                                 data->i;  break;
					case 16:  i16[o++]  = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 16,  data->i); break;
					case 32:  i32[o++]  = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 32,  data->i); break;
					case 64:  i64[o++]  = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 64,  data->i); break;
#ifdef HAS_128BIT
					case 128: i128[o++] = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 128, data->i); break;
#endif
				}
				break;

			case SIGNAL_TYPE_BOOLEAN:
				switch (bits) {
					case  8:  i8 [o++]  =                                                 data->b ? 1 : 0;  break;
					case 16:  i16[o++]  = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 16,  data->b ? 1 : 0); break;
					case 32:  i32[o++]  = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 32,  data->b ? 1 : 0); break;
					case 64:  i64[o++]  = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 64,  data->b ? 1 : 0); break;
#ifdef HAS_128BIT
					case 128: i128[o++] = SWAP_INT_HTOX(io->flags & RAW_BIG_ENDIAN, 128, data->b ? 1 : 0); break;
#endif
				}
				break;

			case SIGNAL_TYPE_COMPLEX:
				switch (bits) {
					case  8:  i8 [o++]  = -1; /* Not supported */
					          i8 [o++]  = -1; break;
					case 16:  i16[o++]  = -1; /* Not supported */
					          i16[o++]  = -1; break;

					case 32:  f32[o++]  = SWAP_FLOAT_HTOX(io->flags & RAW_BIG_ENDIAN,  32, creal(data->z));
					          f32[o++]  = SWAP_FLOAT_HTOX(io->flags & RAW_BIG_ENDIAN,  32, cimag(data->z)); break;
					case 64:  f64[o++]  = SWAP_FLOAT_HTOX(io->flags & RAW_BIG_ENDIAN,  64, creal(data->z));
					          f64[o++]  = SWAP_FLOAT_HTOX(io->flags & RAW_BIG_ENDIAN,  64, cimag(data->z)); break;
#ifdef HAS_128BIT
					case 128: f128[o++] = SWAP_FLOAT_HTOX(io->flags & RAW_BIG_ENDIAN, 128, creal(data->z);
					          f128[o++] = SWAP_FLOAT_HTOX(io->flags & RAW_BIG_ENDIAN, 128, cimag(data->z); break;
#endif
				}
				break;

			case SIGNAL_TYPE_AUTO:
			case SIGNAL_TYPE_INVALID:
				return -1;
		}
	}

out:	if (wbytes)
		*wbytes = o * (bits / 8);

	return 1;
}

int raw_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	int8_t     *i8  =  (void *) buf;
	int16_t    *i16 =  (void *) buf;
	int32_t    *i32 =  (void *) buf;
	int64_t    *i64 =  (void *) buf;
	float      *f32 =  (void *) buf;
	double     *f64 =  (void *) buf;
#ifdef HAS_128BIT
	__int128   *i128 = (void *) buf;
	__float128 *f128 = (void *) buf;
#endif

	/* The raw format can not encode multiple samples in one buffer
	 * as there is no support for framing. */
	struct sample *smp = smps[0];

	int o = 0, bits = 1 << (io->flags >> 24);
	int nlen = len / (bits / 8);

	if (cnt > 1)
		return -1;

	if (len % (bits / 8)) {
		warn("Invalid RAW Payload length: %#zx", len);
		return -1;
	}

	if (io->flags & RAW_FAKE_HEADER) {
		if (nlen < o + 3) {
			warn("Received a packet with no fake header. Skipping...");
			return -1;
		}

		switch (bits) {
			case 8:
				smp->sequence          = i8[o++];
				smp->ts.origin.tv_sec  = i8[o++];
				smp->ts.origin.tv_nsec = i8[o++];
				break;

			case 16:
				smp->sequence          = SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 16, i16[o++]);
				smp->ts.origin.tv_sec  = SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 16, i16[o++]);
				smp->ts.origin.tv_nsec = SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 16, i16[o++]);
				break;

			case 32:
				smp->sequence          = SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 32, i32[o++]);
				smp->ts.origin.tv_sec  = SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 32, i32[o++]);
				smp->ts.origin.tv_nsec = SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 32, i32[o++]);
				break;

			case 64:
				smp->sequence          = SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 64, i64[o++]);
				smp->ts.origin.tv_sec  = SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 64, i64[o++]);
				smp->ts.origin.tv_nsec = SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 64, i64[o++]);
				break;

#ifdef HAS_128BIT
			case 128:
				smp->sequence          = SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 128, i128[o++]);
				smp->ts.origin.tv_sec  = SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 128, i128[o++]);
				smp->ts.origin.tv_nsec = SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 128, i128[o++]);
				break;
#endif
		}

		smp->flags = SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_TS_ORIGIN;
	}
	else {
		smp->flags = 0;
		smp->sequence = 0;
		smp->ts.origin.tv_sec  = 0;
		smp->ts.origin.tv_nsec = 0;
	}

	smp->signals = io->signals;

	int i;
	for (i = 0; i < smp->capacity && o < nlen; i++) {
		enum signal_type fmt = sample_format(smp, i);
		union signal_data *data = &smp->data[i];

		switch (fmt) {
			case SIGNAL_TYPE_FLOAT:
				switch (bits) {
					case 8:   data->f = -1; o++; break; /* Not supported */
					case 16:  data->f = -1; o++; break; /* Not supported */

					case 32:  data->f = SWAP_FLOAT_XTOH(io->flags & RAW_BIG_ENDIAN, 32, f32[o++]); break;
					case 64:  data->f = SWAP_FLOAT_XTOH(io->flags & RAW_BIG_ENDIAN, 64, f64[o++]); break;
#ifdef HAS_128BIT
					case 128: data->f = SWAP_FLOAT_XTOH(io->flags & RAW_BIG_ENDIAN, 128, f128[o++]); break;
#endif
				}
				break;

			case SIGNAL_TYPE_INTEGER:
				switch (bits) {
					case 8:   data->i = (int8_t)                                                    i8[o++];  break;
					case 16:  data->i = (int16_t)  SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN,  16,  i16[o++]); break;
					case 32:  data->i = (int32_t)  SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN,  32,  i32[o++]); break;
					case 64:  data->i = (int64_t)  SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN,  64,  i64[o++]); break;
#ifdef HAS_128BIT
					case 128: data->i = (__int128) SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 128, i128[o++]); break;
#endif
				}
				break;

			case SIGNAL_TYPE_BOOLEAN:
				switch (bits) {
					case 8:   data->b = (bool)                                                  i8[o++];  break;
					case 16:  data->b = (bool) SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN,  16,  i16[o++]); break;
					case 32:  data->b = (bool) SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN,  32,  i32[o++]); break;
					case 64:  data->b = (bool) SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN,  64,  i64[o++]); break;
#ifdef HAS_128BIT
					case 128: data->b = (bool) SWAP_INT_XTOH(io->flags & RAW_BIG_ENDIAN, 128, i128[o++]); break;
#endif
				}
				break;

			case SIGNAL_TYPE_COMPLEX:
				switch (bits) {
					case 8:  data->z = CMPLXF(-1, -1); o += 2; break; /* Not supported */
					case 16: data->z = CMPLXF(-1, -1); o += 2; break; /* Not supported */

					case 32: data->z = CMPLXF(
							SWAP_FLOAT_XTOH(io->flags & RAW_BIG_ENDIAN, 32, f32[o++]), /* real */
							SWAP_FLOAT_XTOH(io->flags & RAW_BIG_ENDIAN, 32, f32[o++])  /* imag */
					        );
						break;

					case 64: data->z = CMPLXF(
							SWAP_FLOAT_XTOH(io->flags & RAW_BIG_ENDIAN, 64, f64[o++]), /* real */
							SWAP_FLOAT_XTOH(io->flags & RAW_BIG_ENDIAN, 64, f64[o++])  /* imag */
					        );
						break;

#if HAS_128BIT
					case 128: data->z = CMPLXF(
							SWAP_FLOAT_XTOH(io->flags & RAW_BIG_ENDIAN, 128, f128[o++]), /* real */
							SWAP_FLOAT_XTOH(io->flags & RAW_BIG_ENDIAN, 128, f128[o++])  /* imag */
					        );
						break;
#endif
				}
				break;

			case SIGNAL_TYPE_AUTO:
			case SIGNAL_TYPE_INVALID:
				warn("Unsupported format in RAW payload");
				return -1;
		}
	}

	smp->length = i;

	if (smp->length > smp->capacity) {
		warn("Received more values than supported: length=%u, capacity=%u", smp->length, smp->capacity);
		smp->length = smp->capacity;
	}

	if (rbytes)
		*rbytes = o * (bits / 8);

	return 1;
}

#define REGISTER_FORMAT_RAW(i, n, d, f)		\
static struct plugin i = {			\
	.name = n,				\
	.description = d,			\
	.type = PLUGIN_TYPE_FORMAT,		\
	.format = {				\
		.flags = f | IO_HAS_BINARY_PAYLOAD |\
			     SAMPLE_HAS_DATA,	\
		.sprint = raw_sprint,		\
		.sscan  = raw_sscan		\
	}					\
};						\
REGISTER_PLUGIN(& i);

/* Feel free to add additional format identifiers here to suit your needs */
REGISTER_FORMAT_RAW(p_8,	"raw.8",	"Raw  8 bit",					RAW_BITS_8)
REGISTER_FORMAT_RAW(p_16be,	"raw.16.be",	"Raw 16 bit, big endian byte-order",		RAW_BITS_16 | RAW_BIG_ENDIAN)
REGISTER_FORMAT_RAW(p_32be,	"raw.32.be",	"Raw 32 bit, big endian byte-order",		RAW_BITS_32 | RAW_BIG_ENDIAN)
REGISTER_FORMAT_RAW(p_64be,	"raw.64.be",	"Raw 64 bit, big endian byte-order",		RAW_BITS_64 | RAW_BIG_ENDIAN)

REGISTER_FORMAT_RAW(p_16le,	"raw.16.le",	"Raw 16 bit, little endian byte-order",		RAW_BITS_16)
REGISTER_FORMAT_RAW(p_32le,	"raw.32.le",	"Raw 32 bit, little endian byte-order",		RAW_BITS_32)
REGISTER_FORMAT_RAW(p_64le,	"raw.64.le",	"Raw 64 bit, little endian byte-order",		RAW_BITS_64)

#ifdef HAS_128BIT
REGISTER_FORMAT_RAW(p_128le,	"raw.128.be",	"Raw 128 bit, big endian byte-order",		RAW_BITS_128 | RAW_BIG_ENDIAN)
REGISTER_FORMAT_RAW(p_128le,	"raw.128.le",	"Raw 128 bit, little endian byte-order",	RAW_BITS_128)
#endif

REGISTER_FORMAT_RAW(p_gtnet,	"gtnet",	"RTDS GTNET",					RAW_BITS_32 | RAW_BIG_ENDIAN)
REGISTER_FORMAT_RAW(p_gtnef,	"gtnet.fake",	"RTDS GTNET with fake header",			RAW_BITS_32 | RAW_BIG_ENDIAN | RAW_FAKE_HEADER)
