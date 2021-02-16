/** Signal metadata list.
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
#include <villas/signal_type.h>
#include <villas/signal_data.h>
#include <villas/signal_list.h>
#include <villas/list.h>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::utils;

int signal_list_init(struct vlist *list)
{
	int ret;

	ret = vlist_init(list);
	if (ret)
		return ret;

	return 0;
}

int signal_list_clear(struct vlist *list)
{
	for (size_t i = 0; i < vlist_length(list); i++) {
		struct signal *sig = (struct signal *) vlist_at(list, i);

		signal_decref(sig);
	}

	vlist_clear(list);

	return 0;
}

int signal_list_destroy(struct vlist *list)
{
	int ret;

	ret = vlist_destroy(list, (dtor_cb_t) signal_decref, false);
	if (ret)
		return ret;

	return 0;
}

int signal_list_parse(struct vlist *list, json_t *json)
{
	int ret;
	struct signal *s;

	if (!json_is_array(json))
		return -1;

	size_t i;
	json_t *json_signal;
	json_array_foreach(json, i, json_signal) {
		s = new struct signal;
		if (!s)
			throw MemoryAllocationError();

		ret = signal_init(s);
		if (ret)
			return ret;

		ret = signal_parse(s, json_signal);
		if (ret)
			return ret;

		vlist_push(list, s);
	}

	return 0;
}

int signal_list_generate(struct vlist *list, unsigned len, enum SignalType typ)
{
	char name[32];

	for (unsigned i = 0; i < len; i++) {
		snprintf(name, sizeof(name), "signal%u", i);

		struct signal *sig = signal_create(name, nullptr, typ);
		if (!sig)
			return -1;

		vlist_push(list, sig);
	}

	return 0;
}

int signal_list_generate2(struct vlist *list, const char *dt)
{
	int len, i = 0;
	char name[32], *e;
	enum SignalType typ;

	for (const char *t = dt; *t; t = e + 1) {
		len = strtoul(t, &e, 10);
		if (t == e)
			len = 1;

		typ = signal_type_from_fmtstr(*e);
		if (typ == SignalType::INVALID)
			return -1;

		for (int j = 0; j < len; j++) {
			snprintf(name, sizeof(name), "signal%d", i++);

			struct signal *sig = signal_create(name, nullptr, typ);
			if (!sig)
				return -1;

			vlist_push(list, sig);
		}
	}

	return 0;
}

void signal_list_dump(Logger logger, const struct vlist *list, const union signal_data *data, unsigned len)
{
	for (size_t i = 0; i < vlist_length(list); i++) {
		struct signal *sig = (struct signal *) vlist_at(list, i);

		char *buf = strf("    %d:", i);

		if (sig->name)
			strcatf(&buf, " %s", sig->name);

		if (sig->unit)
			strcatf(&buf, " [%s]", sig->unit);

		strcatf(&buf, "(%s)", signal_type_to_str(sig->type));

		if (data && i < len) {
			char val[32];

			signal_data_print_str(&data[i], sig->type, val, sizeof(val));

			strcatf(&buf, " = %s", val);
		}

		logger->debug("{}", buf);
		free(buf);
	}
}

int signal_list_copy(struct vlist *dst, const struct vlist *src)
{
	assert(src->state == State::INITIALIZED);
	assert(dst->state == State::INITIALIZED);

	for (size_t i = 0; i < vlist_length(src); i++) {
		struct signal *sig = (struct signal *) vlist_at_safe(src, i);

		signal_incref(sig);

		vlist_push(dst, sig);
	}

	return 0;
}

json_t * signal_list_to_json(struct vlist *list)
{
	json_t *json_signals = json_array();

	for (size_t i = 0; i < vlist_length(list); i++) {
		struct signal *sig = (struct signal *) vlist_at_safe(list, i);

		json_t *json_signal = json_pack("{ s: s, s: b }",
			"type", signal_type_to_str(sig->type),
			"enabled", sig->enabled
		);

		if (sig->name)
			json_object_set_new(json_signal, "name", json_string(sig->name));

		if (sig->unit)
			json_object_set_new(json_signal, "unit", json_string(sig->unit));

		if (!sig->init.is_nan())
			json_object_set_new(json_signal, "init", signal_data_to_json(&sig->init, sig->type));

		json_array_append_new(json_signals, json_signal);
	}

	return json_signals;
}
