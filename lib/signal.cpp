/** Signal meta data.
 *
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

#include <villas/signal.h>
#include <villas/utils.hpp>
#include <villas/mapping.h>
#include <villas/exceptions.hpp>

#include <villas/signal_type.h>
#include <villas/signal_data.h>

using namespace villas;
using namespace villas::utils;

int signal_init(struct signal *s)
{
	s->enabled = true;

	s->name = nullptr;
	s->unit = nullptr;
	s->type = SignalType::INVALID;
	s->init = signal_data::nan();

	s->refcnt = ATOMIC_VAR_INIT(1);

	return 0;
}

int signal_init_from_mapping(struct signal *s, const struct mapping_entry *me, unsigned index)
{
	int ret;

	ret = signal_init(s);
	if (ret)
		return ret;

	ret = mapping_entry_to_str(me, index, &s->name);
	if (ret)
		return ret;

	switch (me->type) {
		case MappingType::STATS:
			s->type = Stats::types[me->stats.type].signal_type;
			break;

		case MappingType::HEADER:
			switch (me->header.type) {
				case MappingHeaderType::LENGTH:
				case MappingHeaderType::SEQUENCE:
					s->type = SignalType::INTEGER;
					break;
			}
			break;

		case MappingType::TIMESTAMP:
			s->type = SignalType::INTEGER;
			break;

		case MappingType::DATA:
			s->type = me->data.signal->type;
			s->init = me->data.signal->init;
			s->enabled = me->data.signal->enabled;

			if (me->data.signal->name)
				s->name = strdup(me->data.signal->name);

			if (me->data.signal->unit)
				s->name = strdup(me->data.signal->unit);
			break;

		case MappingType::UNKNOWN:
			return -1;
	}

	return 0;
}

int signal_destroy(struct signal *s)
{
	if (s->name)
		free(s->name);

	if (s->unit)
		free(s->unit);

	return 0;
}

struct signal * signal_create(const char *name, const char *unit, enum SignalType fmt)
{
	int ret;
	struct signal *sig;

	sig = new struct signal;
	if (!sig)
		throw MemoryAllocationError();

	ret = signal_init(sig);
	if (ret)
		return nullptr;

	if (name)
		sig->name = strdup(name);

	if (unit)
		sig->unit = strdup(unit);

	sig->type = fmt;

	return sig;
}

int signal_free(struct signal *s)
{
	int ret;

	ret = signal_destroy(s);
	if (ret)
		 return ret;

	delete s;

	return 0;
}

int signal_incref(struct signal *s)
{
	return atomic_fetch_add(&s->refcnt, 1) + 1;
}

int signal_decref(struct signal *s)
{
	int prev = atomic_fetch_sub(&s->refcnt, 1);

	/* Did we had the last reference? */
	if (prev == 1) {
		int ret = signal_free(s);
		if (ret)
			throw RuntimeError("Failed to release sample");
	}

	return prev - 1;
}

struct signal * signal_copy(struct signal *s)
{
	int ret;
	struct signal *ns;

	ns = new struct signal;
	if (!ns)
		throw MemoryAllocationError();

	ret = signal_init(ns);
	if (!ret)
		throw RuntimeError("Failed to initialize signal");

	ns->type = s->type;
	ns->init = s->init;
	ns->enabled = s->enabled;

	if (s->name)
		ns->name = strdup(s->name);

	if (s->unit)
		ns->name = strdup(s->unit);

	return ns;
}

int signal_parse(struct signal *s, json_t *json)
{
	int ret;
	json_error_t err;
	json_t *json_init = nullptr;
	const char *name = nullptr;
	const char *unit = nullptr;
	const char *type = "float";

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: s, s?: o, s?: b }",
		"name", &name,
		"unit", &unit,
		"type", &type,
		"init", &json_init,
		"enabled", &s->enabled
	);
	if (ret)
		return -1;

	if (name)
		s->name = strdup(name);

	if (unit)
		s->unit = strdup(unit);

	if (type) {
		s->type = signal_type_from_str(type);
		if (s->type == SignalType::INVALID)
			return -1;
	}

	if (json_init) {
		ret = signal_data_parse_json(&s->init, s->type, json_init);
		if (ret)
			return ret;
	}
	else
		signal_data_set(&s->init, s->type, 0);

	return 0;
}

json_t * signal_to_json(struct signal *s)
{
	json_t *json_sig = json_pack("{ s: s, s: b, s: o }",
		"type", signal_type_to_str(s->type),
		"enabled", s->enabled,
		"init", signal_data_to_json(&s->init, s->type)
	);

	if (s->name)
		json_object_set(json_sig, "name", json_string(s->name));

	if (s->unit)
		json_object_set(json_sig, "unit", json_string(s->unit));

	return json_sig;
}
