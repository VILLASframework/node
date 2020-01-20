
/** Common code.
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

#include <villas/common.h>

#include <cstdlib>

const char * state_print(enum State s)
{
	switch (s) {
		case State::DESTROYED:
			return "destroyed";
			break;

		case State::INITIALIZED:
			return "initialized";
			break;

		case State::PARSED:
			return "parsed";
			break;

		case State::CHECKED:
			return "checked";
			break;

		case State::STARTED:
			return "running";
			break;

		case State::STOPPED:
			return "stopped";
			break;

		case State::PENDING_CONNECT:
			return "pending-connect";
			break;

		case State::CONNECTED:
			return "connected";
			break;

		case State::PAUSED:
			return "paused";
			break;

		default:
			return nullptr;
	}
}
