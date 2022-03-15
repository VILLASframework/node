/** Signal meta data.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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
	json_t *json_sig = json_pack("{ s: s, s: o }",
		"type", signalTypeToString(type).c_str(),
		"init", init.toJson(type)
	);

	if (!name.empty())
		json_object_set(json_sig, "name", json_string(name.c_str()));

	if (!unit.empty())
		json_object_set(json_sig, "unit", json_string(unit.c_str()));

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

/** Check if two signal names are numbered ascendingly
 *
 * E.g. signal3 -> signal4
 */
static
bool isNextName(const std::string &a, const std::string &b)
{
	/* Find common prefix */
	std::string::const_iterator ia, ib;
	for (ia = a.cbegin(), ib = b.cbegin();
	     ia != b.cend() && ib != b.cend() && *ia == *ib;
	     ++ia, ++ib);

	/* Suffixes */
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
