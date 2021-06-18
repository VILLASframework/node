/** RMS hook.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
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

/** @addtogroup hooks Hook functions
 * @{
 */

#include <cstring>
#include <cinttypes>
#include <vector>
#include <villas/timing.h>
#include <villas/hook.hpp>
#include <villas/path.h>
#include <villas/sample.h>

namespace villas {
namespace node {

class RMSHook : public Hook {

protected:
	std::vector<std::vector<double>> smpMemory;

	uint64_t calcCount;
	unsigned rate;
	unsigned windowSize;
	bool sync;
	double nextCalc;

	uint64_t smpMemPos;
	uint64_t lastSequence;
	struct timespec lastCalc;

	std::vector<int> signalIndex;	/**< A list of signalIndex to do rms on */

public:
	RMSHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		smpMemory(),
		calcCount(0),
		rate(0),
		windowSize(0),
		sync(0),
		nextCalc(0.0),
		smpMemPos(0),
		lastSequence(0),
		lastCalc({0, 0}),
		signalIndex()
	{ }

	virtual void prepare()
	{
		signal_list_clear(&signals);
		for (unsigned i = 0; i < signalIndex.size(); i++) {
			struct signal *amplSig;

			/* Add signals */
			amplSig = signal_create("rms", "", SignalType::FLOAT);

			if (!amplSig)
				throw RuntimeError("Failed to create new signals");

			vlist_push(&signals, amplSig);
		}

		/* Initialize sample memory */
		smpMemory.clear();
		for (unsigned i = 0; i < signalIndex.size(); i++)
			smpMemory.emplace_back(std::vector<double>(windowSize, 0.0));

		state = State::PREPARED;
	}

	virtual void parse(json_t *cfg)
	{
		int ret;
		json_error_t err;

		json_t *jsonChannelList = nullptr;

		assert(state != State::STARTED);

		Hook::parse(cfg);

		ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: b, s?: i, s?: o }",
			"window_size", &windowSize,
			"sync", &sync,
			"rate", &rate,
			"signal_index", &jsonChannelList
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-rms");

		if (jsonChannelList != nullptr) {
			signalIndex.clear();
			if (jsonChannelList->type == JSON_ARRAY) {
				size_t i;
				json_t *jsonValue;
				json_array_foreach(jsonChannelList, i, jsonValue) {
					if (!json_is_number(jsonValue))
						throw ConfigError(jsonValue, "node-config-hook-rms-channel", "Values must be given as array of integer values!");

					auto idx = json_number_value(jsonValue);

					signalIndex.push_back(idx);
				}
			}
			else if (jsonChannelList->type == JSON_INTEGER) {
				if (!json_is_number(jsonChannelList))
					throw ConfigError(jsonChannelList, "node-config-hook-rms-channel", "Value must be given as integer value!");

				auto idx = json_number_value(jsonChannelList);

				signalIndex.push_back(idx);
			}
			else
				logger->warn("Could not parse channel list. Please check documentation for syntax");
		}
		else
			throw ConfigError(jsonChannelList, "node-config-node-signal", "No parameter signalIndex given.");

		state = State::PARSED;
	}

	virtual Hook::Reason process(struct sample *smp)
	{
		assert(state == State::STARTED);

		for (unsigned i = 0; i < signalIndex.size(); i++)
			smpMemory[i][smpMemPos % windowSize] = pow(smp->data[signalIndex[i]].f, 2);

		smpMemPos++;

		bool runRms = false;
		if (sync) {
			double smpNsec = smp->ts.origin.tv_sec * 1e9 + smp->ts.origin.tv_nsec;

			if (smpNsec > nextCalc) {
				runRms = true;
				nextCalc = (( smp->ts.origin.tv_sec ) + ( ((calcCount % rate) + 1) / (double)rate )) * 1e9;
			}
		}

		if (runRms) {
			lastCalc = smp->ts.origin;

			for (unsigned i = 0; i < signalIndex.size(); i++) {
				double rms = 0;

				for (unsigned j = 0; j < windowSize; j++)
					rms += smpMemory[i][j];

				rms = pow(rms / windowSize, 0.5) ;


				if (calcCount > 1) {
					smp->data[i * 4 + 0].f = rms; /* RMS */
				}
			}

			smp->length = calcCount > 1 ? signalIndex.size() * 4 : 0;

			calcCount++;
		}

		if (smp->sequence - lastSequence > 1)
			logger->warn("Calculation is not Realtime. {} sampled missed", smp->sequence - lastSequence);

		lastSequence = smp->sequence;

		return runRms
			? Reason::OK
			: Reason::SKIP_SAMPLE;
	}


};

/* Register hook */
static char n[] = "rms";
static char d[] = "This hook calculates the root-mean-square (RMS) on a window";
static HookPlugin<RMSHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

/** @} */
