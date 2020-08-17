/** Signal meta data.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <atomic>

#include <villas/signal_list.h>
#include <villas/signal_type.h>
#include <villas/signal_data.h>

/* "I" defined by complex.h collides with a define in OpenSSL */
#undef I

/* Forward declarations */
struct mapping_entry;

/** Signal descriptor.
 *
 * This data structure contains meta data about samples values in struct sample::data
 */
struct signal {
	char *name;		/**< The name of the signal. */
	char *unit;		/**< The unit of the signal. */

	union signal_data init;	/**< The initial value of the signal. */

	int enabled;

	std::atomic<int> refcnt;	/**< Reference counter. */

	enum SignalType type;
};

/** Initialize a signal with default values. */
int signal_init(struct signal *s);

/** Destroy a signal and release memory. */
int signal_destroy(struct signal *s);

/** Allocate memory for a new signal, and initialize it with provided values. */
struct signal * signal_create(const char *name, const char *unit, enum SignalType fmt);

/** Destroy and release memory of signal. */
int signal_free(struct signal *s);

/** Increase reference counter. */
int signal_incref(struct signal *s);

/** Decrease reference counter. */
int signal_decref(struct signal *s);

/** Copy a signal. */
struct signal * signal_copy(struct signal *s);

/** Parse signal description. */
int signal_parse(struct signal *s, json_t *cfg);

/** Initialize signal from a mapping_entry. */
int signal_init_from_mapping(struct signal *s, const struct mapping_entry *me, unsigned index);

/** Produce JSON representation of signal. */
json_t * signal_to_json(struct signal *s);
