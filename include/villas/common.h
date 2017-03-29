/** Some common defines, enums and datastructures.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

/* Common states for most objects in VILLASnode (paths, nodes, hooks, plugins) */
enum state {
	STATE_DESTROYED		= 0,
	STATE_INITIALIZED	= 1,
	STATE_PARSED		= 2,
	STATE_CHECKED		= 3,
	STATE_STARTED		= 4,
	STATE_LOADED		= 4, /* alias for STATE_STARTED used by plugins */
	STATE_STOPPED		= 5,
	STATE_UNLOADED		= 5 /* alias for STATE_STARTED used by plugins */
};