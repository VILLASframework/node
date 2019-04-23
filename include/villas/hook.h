/** Hook functions
 *
 * Every node or path can register hook functions which is called for every
 * processed sample. This can be used to debug the data flow, get statistics
 * or alter the sample contents.
 *
 * @file
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
 */

/**
 * @addtogroup hooks User-defined hook functions
 * @ingroup path
 * @{
 *********************************************************************************/

#pragma once

enum hook_flags {
	HOOK_BUILTIN	= (1 << 0), /**< Should we add this hook by default to every path?. */
	HOOK_PATH	= (1 << 1), /**< This hook type is used by paths. */
	HOOK_NODE_READ	= (1 << 2), /**< This hook type is used by nodes. */
	HOOK_NODE_WRITE	= (1 << 3)  /**< This hook type is used by nodes. */
};

enum hook_reason {
	HOOK_OK,
	HOOK_ERROR,
	HOOK_SKIP_SAMPLE,
	HOOK_STOP_PROCESSING
};
