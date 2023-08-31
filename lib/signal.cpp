/* Signal meta data.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/signal.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>

#include <villas/signal_type.hpp>
#include <villas/signal_data.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

Signal::Signal(const std::string &n, const std::string &u, enum SignalType t) :
	name(n),
	unit(u),
	init(SignalData::nan()),
	type(t)
{ }

int Signal::parse(json_t *json)
{
	int ret;
	json_error_t err;
	json_t *json_init = nullptr;
	const char *name_str = nullptr;
	const char *unit_str = nullptr;
	const char *type_str = "float";

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: s, s?: o }",
		"name", &name_str,
		"unit", &unit_str,
		"type", &type_str,
		"init", &json_init
	);
	if (ret)
		return -1;

	if (name_str)
		name = name_str;

	if (unit_str)
		unit = unit_str;

	if (type_str) {
		type = signalTypeFromString(type_str);
		if (type == SignalType::INVALID)
			return -1;
	}

	if (json_init) {
		ret = init.parseJson(type, json_init);
		if (ret)
			return ret;
	}
	else
		init.set(type, 0);

	return 0;
}

json_t * Signal::toJson() const
{
	json_t *json_sig = json_pack("{ s: s }",
		"type", signalTypeToString(type).c_str()
	);

	auto *json_init = init.toJson(type);
	if (json_init)
		json_object_set_new(json_sig, "init", json_init);

	if (!name.empty())
		json_object_set_new(json_sig, "name", json_string(name.c_str()));

	if (!unit.empty())
		json_object_set_new(json_sig, "unit", json_string(unit.c_str()));

	return json_sig;
}

std::string Signal::toString(const union SignalData *d) const
{
	std::stringstream ss;

	if (!name.empty())
		ss << " " << name;

	if (!unit.empty())
		ss << " [" << unit << "]";

	ss << "(" << signalTypeToString(type) << ")";

	if (d)
		ss << " = " << d->toString(type);

	return ss.str();
}

/* Check if two signal names are numbered ascendingly
 *
 * E.g. signal3 -> signal4
 */
static
bool isNextName(const std::string &a, const std::string &b)
{
	// Find common prefix
	std::string::const_iterator ia, ib;
	for (ia = a.cbegin(), ib = b.cbegin();
	     ia != b.cend() && ib != b.cend() && *ia == *ib;
	     ++ia, ++ib);

	// Suffixes
	auto sa = std::string(ia, a.cend());
	auto sb = std::string(ib, b.cend());

	try {
		size_t ea, eb;
		auto na = std::stoul(sa, &ea, 10);
		auto nb = std::stoul(sb, &eb, 10);

		return na + 1 == nb;
	} catch (std::exception &) {
		return false;
	}
}

bool Signal::isNext(const Signal &sig)
{
	if (type != sig.type)
		return false;

	if (!unit.empty() && !sig.unit.empty() && unit != sig.unit)
		return false;

	return isNextName(name, sig.name);
}
