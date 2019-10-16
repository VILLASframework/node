/** Decimate hook.
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

#pragma once

#include <villas/hook.hpp>

/** @addtogroup hooks Hook functions
 * @{
 */

namespace villas {
namespace node {

class DecimateHook : public LimitHook {

protected:
	int ratio;
	unsigned counter;

public:
	using LimitHook::LimitHook;

	virtual void setRate(double rate, double maxRate = -1)
	{
		assert(maxRate > 0);

		int ratio = maxRate / rate;
		if (ratio == 0)
			ratio = 1;

		setRatio(ratio);
	}

	void setRatio(int r)
	{
		ratio = r;
	}

	virtual void start();

	virtual void parse(json_t *cfg);

	virtual Hook::Reason process(sample *smp);
};

} /* namespace node */
} /* namespace villas */

/**
 * @}
 */
