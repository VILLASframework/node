/** Rate-limiting hook.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <villas/hook.hpp>

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
	LimitRateHook(Path *p, Node *n, int fl, int prio, bool en = true) :
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

	virtual Hook::Reason process(struct Sample *smp);
};

} /* namespace node */
} /* namespace villas */
