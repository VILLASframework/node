
/** Common code.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#include <villas/common.hpp>

#include <cstdlib>

std::string stateToString(enum State s)
{
	switch (s) {
		case State::DESTROYED:
			return "destroyed";

		case State::INITIALIZED:
			return "initialized";

		case State::PARSED:
			return "parsed";

		case State::CHECKED:
			return "checked";

		case State::STARTED:
			return "running";

		case State::STOPPED:
			return "stopped";

		case State::PENDING_CONNECT:
			return "pending-connect";

		case State::CONNECTED:
			return "connected";

		case State::PAUSED:
			return "paused";

		default:
			return "";
	}
}
