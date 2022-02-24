
/** Cast hook.
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

#include <cstring>

#include <villas/hook.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class CastHook : public MultiSignalHook {

protected:
	enum SignalType new_type;
	std::string new_name;
	std::string new_unit;

public:
	CastHook(Path *p, Node *n, int fl, int prio, bool en = true) :
		MultiSignalHook(p, n, fl, prio, en),
		new_type(SignalType::INVALID)
	{ }

	virtual void prepare()
	{
		assert(state == State::CHECKED);

		MultiSignalHook::prepare();

		for (auto index : signalIndices) {
			auto orig_sig = signals->getByIndex(index);

			auto type = new_type == SignalType::INVALID ? orig_sig->type : new_type;
			auto name = new_name.empty()                ? orig_sig->name : new_name;
			auto unit = new_unit.empty()                ? orig_sig->unit : new_unit;

			(*signals)[index] = std::make_shared<Signal>(name, unit, type);
		}

		state = State::PREPARED;
	}

	virtual void parse(json_t *json)
	{
		int ret;

		json_error_t err;

		assert(state != State::STARTED);

		MultiSignalHook::parse(json);

		const char *name = nullptr;
		const char *unit = nullptr;
		const char *type = nullptr;

		ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: s }",
			"new_type", &type,
			"new_name", &name,
			"new_unit", &unit
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-cast");

		if (type) {
			new_type = signalTypeFromString(type);
			if (new_type == SignalType::INVALID)
				throw RuntimeError("Invalid signal type: {}", type);
		}
		else
			/* We use this constant to indicate that we dont want to change the type. */
			new_type = SignalType::INVALID;

		if (name)
			new_name = name;

		if (unit)
			new_unit = unit;

		state = State::PARSED;
	}

	virtual Hook::Reason process(struct Sample *smp)
	{
		assert(state == State::STARTED);

		for (auto index : signalIndices) {
			auto orig_sig = smp->signals->getByIndex(index);
			auto new_sig  = signals->getByIndex(index);

			smp->data[index] = smp->data[index].cast(orig_sig->type, new_sig->type);
		}

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "cast";
static char d[] = "Cast signals types";
static HookPlugin<CastHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

