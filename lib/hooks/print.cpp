/** Print hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

/** @addtogroup hooks Hook functions
 * @{
 */

#include <cstdio>
#include <cstring>

#include <villas/node.h>
#include <villas/hook.hpp>
#include <villas/path.h>
#include <villas/sample.h>
#include <villas/format.hpp>
#include <villas/plugin.h>

namespace villas {
namespace node {

class PrintHook : public Hook {

protected:
	Format *formatter;
	FILE *stream;

	std::string prefix;
	std::string uri;

public:
	PrintHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		formatter(nullptr),
		stream(nullptr)
	{ }

	virtual ~PrintHook()
	{
		if (formatter)
			delete formatter;
	}

	virtual void start()
	{
		assert(state == State::PREPARED || state == State::STOPPED);

		formatter->start(&signals);

		stream = !uri.empty() ? fopen(uri.c_str(), "a+") : stdout;
		if (!stream)
			throw SystemError("Failed to open IO");

		state = State::STARTED;
	}

	virtual void stop()
	{
		assert(state == State::STARTED);

		if (stream != stdout)
			fclose(stream);

		state = State::STOPPED;
	}

	virtual void parse(json_t *json)
	{
		const char *p = nullptr, *u = nullptr;
		int ret;
		json_error_t err;
		json_t *json_format = nullptr;

		assert(state != State::STARTED);

		Hook::parse(json);

		ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: o }",
			"output", &u,
			"prefix", &p,
			"format", &json_format
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-print");

		if (p)
			prefix = p;

		if (u)
			uri = u;

		/* Format */
		formatter = json_format
				? FormatFactory::make(json_format)
				: FormatFactory::make("villas.human");
		if (!formatter)
			throw ConfigError(json_format, "node-config-hook-print-format", "Invalid format configuration");

		state = State::PARSED;
	}

	virtual Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);

		if (stream == stdout) {
			if (!prefix.empty())
				printf("%s", prefix.c_str());
			else if (node)
				printf("Node %s: ", node_name(node));
			else if (path)
				printf("Path %s: ", path_name(path));
		}

		formatter->print(stream, smp);

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "print";
static char d[] = "Print the message to stdout";
static HookPlugin<PrintHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

/** @} */

