/** Skip first hook.
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

/** @addtogroup hooks Hook functions
 * @{
 */

#include <villas/hook.hpp>
#include <villas/timing.h>
#include <villas/sample.h>

namespace villas {
namespace node {

class SkipFirstHook : public Hook {

protected:
	enum class SkipState {
		STARTED,	/**< Path just started. First sample not received yet. */
		SKIPPING,	/**< First sample received. Skipping samples now. */
		NORMAL		/**< All samples skipped. Normal operation. */
	} skip_state;

	enum class Mode {
		SECONDS,
		SAMPLES
	} mode;

	union {
		struct {
			timespec until;
			timespec wait;	/**< Absolute point in time from where we accept samples. */
		} seconds;

		struct {
			uint64_t until;
			int wait;
		} samples;
	};

public:
	using Hook::Hook;

	virtual void parse(json_t *cfg)
	{
		double s;

		int ret;
		json_error_t err;

		assert(state != State::STARTED);

		Hook::parse(cfg);

		ret = json_unpack_ex(cfg, &err, 0, "{ s: F }", "seconds", &s);
		if (!ret) {
			seconds.wait = time_from_double(s);
			mode = Mode::SECONDS;

			state = State::PARSED;
			return;
		}

		ret = json_unpack_ex(cfg, &err, 0, "{ s: i }", "samples", &samples.wait);
		if (!ret) {
			mode = Mode::SAMPLES;

			state = State::PARSED;
			return;
		}

		throw ConfigError(cfg, err, "node-config-hook-skip_first");
	}

	virtual void start()
	{
		skip_state = SkipState::STARTED;
		state = State::STARTED;
	}

	virtual void restart()
	{
		skip_state = SkipState::STARTED;
	}

	virtual Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);

		/* Remember sequence no or timestamp of first sample. */
		if (skip_state == SkipState::STARTED) {
			switch (mode) {
				case Mode::SAMPLES:
					samples.until = smp->sequence + samples.wait;
					break;

				case Mode::SECONDS:
					seconds.until = time_add(&smp->ts.origin, &seconds.wait);
					break;
			}

			skip_state = SkipState::SKIPPING;
		}

		switch (mode) {
			case Mode::SAMPLES:
				if (samples.until > smp->sequence)
					return Hook::Reason::SKIP_SAMPLE;
				break;

			case Mode::SECONDS:
				if (time_delta(&seconds.until, &smp->ts.origin) < 0)
					return Hook::Reason::SKIP_SAMPLE;
				break;

			default:
				break;
		}

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "skip_first";
static char d[] = "Skip the first samples";
static HookPlugin<SkipFirstHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

/** @} */
