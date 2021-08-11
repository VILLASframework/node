/** Some common defines, enums and datastructures.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#include <string>

/* Common states for most objects in VILLAScommon (paths, nodes, hooks, plugins) */
enum class State {
	DESTROYED	= 0,
	INITIALIZED	= 1,
	PARSED		= 2,
	CHECKED		= 3,
	STARTED		= 4,
	STOPPED		= 5,
	PENDING_CONNECT	= 6,
	CONNECTED	= 7,
	PAUSED		= 8,
	STARTING	= 9,
	STOPPING	= 10,
	PAUSING		= 11,
	RESUMING	= 12,
	PREPARED	= 13
};

/** Callback to destroy list elements.
 *
 * @param data A pointer to the data which should be freed.
 */
typedef int (*dtor_cb_t)(void *);

/** Convert state enum to human readable string. */
std::string stateToString(enum State s);
