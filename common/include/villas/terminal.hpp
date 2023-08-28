/* Terminal handling.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 */

#pragma once

#include <cstdio>

#include <signal.h>
#include <sys/ioctl.h>

namespace villas {

class Terminal {

protected:
	struct winsize window;	// Size of the terminal window.

	bool isTty;

	static
	class Terminal *current;

public:
	Terminal();

	// Signal handler for TIOCGWINSZ
	static
	void resize(int signal, siginfo_t *sinfo, void *ctx);

	static
	int getCols()
	{
		if (!current)
			current = new Terminal();

		return current->window.ws_col;
	}

	static
	int getRows()
	{
		if (!current)
			current = new Terminal();

		return current->window.ws_row;
	}
};

} // namespace villas
