
/** Moving average filter.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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

/** @addtogroup hooks Hook functions
 * @{
 */

#include <villas/hook.hpp>
#include <villas/sample.h>

namespace villas {
namespace node {

class MovingAverageHook : public MultiSignalHook {

protected:
	std::vector<std::vector<double>> smpMemory;

	double accumulator;
	unsigned windowSize;
	uint64_t smpMemoryPosition;

public:
	MovingAverageHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		MultiSignalHook(p, n, fl, prio, en),
		smpMemory(),
		accumulator(0.0),
		windowSize(0),
		smpMemoryPosition(0)
	{ }

	virtual
	void prepare()
	{
		MultiSignalHook::prepare();

		/* Add signals */
		for (auto index : signalIndices) {
			auto *origSig = (struct signal *) vlist_at(&signals, index);

			/* Check that signal has float type */
			if (origSig->type != SignalType::FLOAT)
				throw RuntimeError("The ma hook can only operate on signals of type float!");
		}

		/* Initialize sample memory */
		smpMemory.clear();
		for (unsigned i = 0; i < signalIndices.size(); i++)
			smpMemory.emplace_back(windowSize, 0.0);

		state = State::PREPARED;
	}

	virtual
	void parse(json_t *json)
	{
		int ret;
		json_error_t err;

		assert(state != State::STARTED);

		MultiSignalHook::parse(json);

		ret = json_unpack_ex(json, &err, 0, "{ s?: i }",
			"window_size", &windowSize
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-rms");

		state = State::PARSED;
	}

	virtual
	Hook::Reason process(struct sample *smp)
	{
		assert(state == State::STARTED);

		unsigned i = 0;
		for (auto index : signalIndices) {
			/* The new value */
			double newValue = smp->data[index].f;

			/* Append the new value to the history memory */
			smpMemory[i][smpMemoryPosition % windowSize] = newValue;

			/* Get the old value from the history */
			double oldValue = smpMemory[i][(smpMemoryPosition + 1) % windowSize];

			/* Update the accumulator */
			accumulator += newValue;
			accumulator -= oldValue;

			smp->data[index].f = accumulator / windowSize;
			i++;
		}

		smpMemoryPosition++;

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "ma";
static char d[] = "A simple moving average filter over a fixed number of past samples";
static HookPlugin<MovingAverageHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

/** @} */
