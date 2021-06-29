/** Rate-limiting hook.
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

#pragma once

#include <villas/hook.hpp>

/** @addtogroup hooks Hook functions
 * @{
 */

namespace villas {
namespace node {

class LimitRateHook : public LimitHook {

protected:
	enum {
		LIMIT_RATE_LOCAL,
		LIMIT_RATE_RECEIVED,
		LIMIT_RATE_ORIGIN
	} mode; /**< The timestamp which should be used for limiting. */

	double deadtime;
	timespec last;

public:
	LimitRateHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		LimitHook(p, n, fl, prio, en),
		mode(LIMIT_RATE_LOCAL),
		deadtime(0),
		last({0, 0})
	{ }

	virtual void setRate(double rate, double maxRate = -1)
	{
		deadtime = 1.0 / rate;
	}

	virtual void parse(json_t *json);

	virtual Hook::Reason process(struct sample *smp);
};


} /* namespace node */
} /* namespace villas */

/**
 * @}
 */
