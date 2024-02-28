/** Some common defines, enums and datastructures.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#ifdef __cplusplus
extern "C" {
#endif

/* Common states for most objects in VILLAScommon (paths, nodes, hooks, plugins) */
enum state {
	STATE_DESTROYED		= 0,
	STATE_INITIALIZED	= 1,
	STATE_PARSED		= 2,
	STATE_CHECKED		= 3,
	STATE_STARTED		= 4,
	STATE_LOADED		= 4, /* alias for STATE_STARTED used by struct plugin */
	STATE_OPENED		= 4, /* alias for STATE_STARTED used by struct io */
	STATE_STOPPED		= 5,
	STATE_UNLOADED		= 5, /* alias for STATE_STARTED used by struct plugin */
	STATE_CLOSED		= 5, /* alias for STATE_STARTED used by struct io */
	STATE_PENDING_CONNECT	= 6,
	STATE_CONNECTED		= 7,
	STATE_PAUSED		= 8
};

/** Callback to destroy list elements.
 *
 * @param data A pointer to the data which should be freed.
 */
typedef int (*dtor_cb_t)(void *);

/** Convert state enum to human readable string. */
const char * state_print(enum state s);

#ifdef __cplusplus
}
#endif
