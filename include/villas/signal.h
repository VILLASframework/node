/** Signal meta data.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <jansson.h>
#include <complex.h>
#include <stdint.h>
#include <stdbool.h>

/* "I" defined by complex.h collides with a define in OpenSSL */
#undef I

#include <villas/atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct list;
struct node;
struct mapping_entry;

/** A signal value.
 *
 * Data is in host endianess!
 */
union signal_data {
	double f;			/**< Floating point values. */
	int64_t i;			/**< Integer values. */
	bool b;				/**< Boolean values. */
	float _Complex z;		/**< Complex values. */
};

enum signal_format {
	SIGNAL_FORMAT_INVALID	= 0,	/**< Signal format is invalid. */
	SIGNAL_FORMAT_AUTO	= 1,	/**< Signal format is unknown. Try autodetection. */
	SIGNAL_FORMAT_FLOAT	= 2,	/**< See signal_data::f */
	SIGNAL_FORMAT_INT	= 3,	/**< See signal_data::i */
	SIGNAL_FORMAT_BOOL 	= 4,	/**< See signal_data::b */
	SIGNAL_FORMAT_COMPLEX 	= 5	/**< See signal_data::z */
};

/** Signal descriptor.
 *
 * This data structure contains meta data about samples values in struct sample::data
 */
struct signal {
	char *name;		/**< The name of the signal. */
	char *unit;		/**< The unit of the signal. */

	union signal_data init;	/**< The initial value of the signal. */

	int enabled;

	atomic_int refcnt;	/**< Reference counter. */

	enum signal_format format;
};

/** Initialize a signal with default values. */
int signal_init(struct signal *s);

/** Destroy a signal and release memory. */
int signal_destroy(struct signal *s);

/** Allocate memory for a new signal, and initialize it with provided values. */
struct signal * signal_create(const char *name, const char *unit, enum signal_format fmt);

/** Destroy and release memory of signal. */
int signal_free(struct signal *s);

/** Increase reference counter. */
int signal_incref(struct signal *s);

/** Decrease reference counter. */
int signal_decref(struct signal *s);

/** Parse signal description. */
int signal_parse(struct signal *s, json_t *cfg);

/** Initialize signal from a mapping_entry. */
int signal_init_from_mapping(struct signal *s, const struct mapping_entry *me, unsigned index);

int signal_list_parse(struct list *list, json_t *cfg);

int signal_list_generate(struct list *list, unsigned len, enum signal_format fmt);

void signal_list_dump(const struct list *list);

enum signal_format signal_format_from_str(const char *str);

const char * signal_format_to_str(enum signal_format fmt);

enum signal_format signal_format_detect(const char *val);

/** Convert signal data from one description/format to another. */
void signal_data_convert(union signal_data *data, const struct signal *from, const struct signal *to);

/** Print value of a signal to a character buffer. */
int signal_data_snprint(const union signal_data *data, const struct signal *sig, char *buf, size_t len);

int signal_data_parse_str(union signal_data *data, const struct signal *sig, const char *ptr, char **end);

int signal_data_parse_json(union signal_data *data, const struct signal *sig, json_t *cfg);

void signal_data_set(union signal_data *data, const struct signal *sig, double val);

#ifdef __cplusplus
}
#endif
