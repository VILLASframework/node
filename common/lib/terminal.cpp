/** Terminal handling.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
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

	isTty = isatty(STDERR_FILENO);

	Logger logger = logging.get("terminal");

	if (isTty) {
		struct sigaction sa_resize;
		sa_resize.sa_flags = SA_SIGINFO;
		sa_resize.sa_sigaction = resize;

		sigemptyset(&sa_resize.sa_mask);

		ret = sigaction(SIGWINCH, &sa_resize, nullptr);
		if (ret)
			throw SystemError("Failed to register signal handler");

		// Try to get initial terminal dimensions
		ret = ioctl(STDERR_FILENO, TIOCGWINSZ, &window);
		if (ret)
			logger->warn("Failed to get terminal dimensions");
	} else {
		logger->info("stderr is not associated with a terminal! Using fallback values for window size...");
	}

	// Fallback if for some reason we can not determine a prober window size
	if (window.ws_col == 0)
		window.ws_col = 150;

	if (window.ws_row == 0)
		window.ws_row = 50;
}

void Terminal::resize(int, siginfo_t *, void *)
{
	if (!current)
		current = new Terminal();

	Logger logger = logging.get("terminal");

	int ret;

	ret = ioctl(STDERR_FILENO, TIOCGWINSZ, &current->window);
	if (ret)
		throw SystemError("Failed to get terminal dimensions");

	logger->debug("New terminal size: {}x{}", current->window.ws_row, current->window.ws_col);
};
