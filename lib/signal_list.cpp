/** Signal metadata list.
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

#include <villas/signal.hpp>
#include <villas/signal_type.hpp>
#include <villas/signal_data.hpp>
#include <villas/signal_list.hpp>
#include <villas/list.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

int SignalList::parse(json_t *json)
{
	int ret;

	if (!json_is_array(json))
		return -1;

	size_t i;
	json_t *json_signal;
	json_array_foreach(json, i, json_signal) {
		auto sig = std::make_shared<Signal>();
		if (!sig)
			throw MemoryAllocationError();

		ret = sig->parse(json_signal);
		if (ret)
			return ret;

		push_back(sig);
	}

	return 0;
}

SignalList::SignalList(unsigned len, enum SignalType typ)
{
	char name[32];

	for (unsigned i = 0; i < len; i++) {
		snprintf(name, sizeof(name), "signal%u", i);

		auto sig = std::make_shared<Signal>(name, "", typ);
		if (!sig)
			throw RuntimeError("Failed to create signal list");

		push_back(sig);
	}
}

SignalList::SignalList(const char *dt)
{
	int len, i = 0;
	char name[32], *e;
	enum SignalType typ;

	for (const char *t = dt; *t; t = e + 1) {
		len = strtoul(t, &e, 10);
		if (t == e)
			len = 1;

		typ = signalTypeFromFormatString(*e);
		if (typ == SignalType::INVALID)
			throw RuntimeError("Failed to create signal list");

		for (int j = 0; j < len; j++) {
			snprintf(name, sizeof(name), "signal%d", i++);

			auto sig = std::make_shared<Signal>(name, "", typ);
			if (!sig)
				throw RuntimeError("Failed to create signal list");

			push_back(sig);
		}
	}
}

void SignalList::dump(Logger logger, const union SignalData *data, unsigned len) const
{
	const char *pfx;
	bool abbrev = false;

	Signal::Ptr prevSig;
	unsigned i = 0;
	for (auto sig : *this) {
		/* Check if this is a sequence of similar signals which can be abbreviated */
		if (i >= 1 && i < size() - 1) {
			if (prevSig->isNext(*sig)) {
				abbrev = true;
				goto skip;
			}
		}

		if (abbrev) {
			pfx = "...";
			abbrev = false;
		}
		else
			pfx = "   ";

		logger->info(" {}{:>3}: {}", pfx, i, sig->toString(i < len ? &data[i] : nullptr));

skip:		prevSig = sig;
		i++;
	}
}

json_t * SignalList::toJson() const
{
	json_t *json_signals = json_array();

	for (const auto &sig : *this)
		json_array_append_new(json_signals, sig->toJson());

	return json_signals;
}

Signal::Ptr SignalList::getByIndex(unsigned idx)
{
	return this->at(idx);
}

int SignalList::getIndexByName(const std::string &name)
{
	unsigned i = 0;
	for (auto s : *this) {
		if (name == s->name)
			return i;

		i++;
	}

	return -1;
}

Signal::Ptr SignalList::getByName(const std::string &name)
{
	for (auto s : *this) {
		if (name == s->name)
			return s;
	}

	return Signal::Ptr();
}

SignalList::Ptr SignalList::clone()
{
	auto l = std::make_shared<SignalList>();

	for (auto s : *this)
		l->push_back(s);

	return l;
}
