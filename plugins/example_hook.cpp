/** A simple example hook function which can be loaded as a plugin.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/hook.hpp>
#include <villas/path.hpp>

namespace villas {
namespace node {

class ExampleHook : public Hook {

public:
	using Hook::Hook;

	virtual void restart()
	{
		logger->info("The path {} restarted!", *path);
	}
};

/* Register hook */
static char n[] = "example";
static char d[] = "This is just a simple example hook";
static HookPlugin<ExampleHook, n, d, (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */
