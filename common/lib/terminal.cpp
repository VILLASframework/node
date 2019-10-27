/** Terminal handling.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <unistd.h>

#include <villas/terminal.hpp>
#include <villas/exceptions.hpp>
#include <villas/log.hpp>

using namespace villas;

class Terminal * Terminal::current = nullptr;

Terminal::Terminal()
{
	int ret;

	window.ws_row = 0;
	window.ws_col = 0;

	isTty = isatty(fileno(stdin));

	if (isTty) {
		Logger logger = logging.get("terminal");

		struct sigaction sa_resize;
		sa_resize.sa_flags = SA_SIGINFO;
		sa_resize.sa_sigaction = resize;

		sigemptyset(&sa_resize.sa_mask);

		ret = sigaction(SIGWINCH, &sa_resize, nullptr);
		if (ret)
			throw SystemError("Failed to register signal handler");

		/* Try to get initial terminal dimensions */
		ret = ioctl(STDERR_FILENO, TIOCGWINSZ, &window);
		if (ret)
			logger->warn("Failed to get terminal dimensions");
	}

	/* Fallback if for some reason we can not determine a prober window size */
	if (window.ws_col == 0)
		window.ws_col = 150;

	if (window.ws_row == 0)
		window.ws_row = 50;
}

void Terminal::resize(int, siginfo_t *, void *)
{
	int ret;

	ret = ioctl(STDERR_FILENO, TIOCGWINSZ, &current->window);
	if (ret)
		throw SystemError("Failed to get terminal dimensions");

	Logger logger = logging.get("terminal");

	logger->debug("New terminal size: {}x{}", current->window.ws_row, current->window.ws_col);
};
