/** Skip first hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
	enum {
		HOOK_SKIP_FIRST_STATE_STARTED,	/**< Path just started. First sample not received yet. */
		HOOK_SKIP_FIRST_STATE_SKIPPING,	/**< First sample received. Skipping samples now. */
		HOOK_SKIP_FIRST_STATE_NORMAL	/**< All samples skipped. Normal operation. */
	} skip_state;

	enum {
		HOOK_SKIP_MODE_SECONDS,
		HOOK_SKIP_MODE_SAMPLES
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

		assert(state != STATE_STARTED);

		ret = json_unpack_ex(cfg, &err, 0, "{ s: F }", "seconds", &s);
		if (!ret) {
			seconds.wait = time_from_double(s);
			mode = HOOK_SKIP_MODE_SECONDS;

			state = STATE_PARSED;
			return;
		}

		ret = json_unpack_ex(cfg, &err, 0, "{ s: i }", "samples", &samples.wait);
		if (!ret) {
			mode = HOOK_SKIP_MODE_SAMPLES;

			state = STATE_PARSED;
			return;
		}

		throw ConfigError(cfg, err, "node-config-hook-skip_first");
	}

	virtual void restart()
	{
		skip_state = HOOK_SKIP_FIRST_STATE_STARTED;
	}

	virtual int process(sample *smp)
	{
		assert(state == STATE_STARTED);

		/* Remember sequence no or timestamp of first sample. */
		if (skip_state == HOOK_SKIP_FIRST_STATE_STARTED) {
			switch (mode) {
				case HOOK_SKIP_MODE_SAMPLES:
					samples.until = smp->sequence + samples.wait;
					break;

				case HOOK_SKIP_MODE_SECONDS:
					seconds.until = time_add(&smp->ts.origin, &seconds.wait);
					break;
			}

			skip_state = HOOK_SKIP_FIRST_STATE_SKIPPING;
		}

		switch (mode) {
			case HOOK_SKIP_MODE_SAMPLES:
				if (samples.until > smp->sequence)
					return HOOK_SKIP_SAMPLE;
				break;

			case HOOK_SKIP_MODE_SECONDS:
				if (time_delta(&seconds.until, &smp->ts.origin) < 0)
					return HOOK_SKIP_SAMPLE;
				break;

			default:
				break;
		}

		return HOOK_OK;
	}
};

/* Register hook */
static HookPlugin<SkipFirstHook> p(
	"skip_first",
	"Skip the first samples",
	HOOK_NODE_READ | HOOK_NODE_WRITE | HOOK_PATH,
	99
);

} /* namespace node */
} /* namespace villas */

/** @} */
