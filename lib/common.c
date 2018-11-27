
/** Common code.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdlib.h>

const char * state_print(enum state s)
{
	switch (s) {
		case STATE_DESTROYED:
			return "destroyed";
			break;

		case STATE_INITIALIZED:
			return "initialized";
			break;

		case STATE_PARSED:
			return "parsed";
			break;

		case STATE_CHECKED:
			return "checked";
			break;

		case STATE_STARTED:
			return "running";
			break;

		case STATE_STOPPED:
			return "stopped";
			break;

		case STATE_PENDING_CONNECT:
			return "pending-connect";
			break;

		case STATE_CONNECTED:
			return "connected";
			break;

		case STATE_PAUSED:
			return "paused";
			break;

		default:
			return NULL;
	}
}
