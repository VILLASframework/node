/** Terminal handling.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstdio>

#include <signal.h>
#include <sys/ioctl.h>

namespace villas {

class Terminal {

protected:
	struct winsize window;	/**< Size of the terminal window. */

	bool isTty;

	static class Terminal *current;

public:
	Terminal();

	/** Signal handler for TIOCGWINSZ */
	static void resize(int signal, siginfo_t *sinfo, void *ctx);

	static int getCols()
	{
		if (!current)
			current = new Terminal();

		return current->window.ws_col;
	}

	static int getRows()
	{
		if (!current)
			current = new Terminal();

		return current->window.ws_row;
	}
};

} /* namespace villas */
