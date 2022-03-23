/** Round hook.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
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

#include <cstring>

#include <villas/hook.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class RoundHook : public MultiSignalHook {

protected:
	unsigned precision;

public:
	RoundHook(Path *p, Node *n, int fl, int prio, bool en = true) :
		MultiSignalHook(p, n, fl, prio, en),
		precision(1)
	{ }

	virtual void parse(json_t *json)
	{
		int ret;
		json_error_t err;

		assert(state != State::STARTED);

		MultiSignalHook::parse(json);

		ret = json_unpack_ex(json, &err, 0, "{ s?: i}",
			"precision", &precision
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-round");

		state = State::PARSED;
	}

	virtual Hook::Reason process(struct Sample *smp)
	{
		for (auto index : signalIndices) {
			assert(index < smp->length);
			assert(state == State::STARTED);

			switch (sample_format(smp, index)) {
				case SignalType::FLOAT:
					smp->data[index].f = round(smp->data[index].f * pow(10, precision)) / pow(10, precision);
					break;

				case SignalType::COMPLEX:
					smp->data[index].z = std::complex<float>(
						round(smp->data[index].z.real() * pow(10, precision)) / pow(10, precision),
						round(smp->data[index].z.imag() * pow(10, precision)) / pow(10, precision)
					);
					break;
				default: { }
			}
		}

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "round";
static char d[] = "Round signals to a set number of digits";
static HookPlugin<RoundHook, n, d, (int) Hook::Flags::PATH | (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE> p;

} /* namespace node */
} /* namespace villas */
