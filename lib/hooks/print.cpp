/** Print hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstdio>
#include <cstring>

#include <villas/node.hpp>
#include <villas/hook.hpp>
#include <villas/path.hpp>
#include <villas/sample.hpp>
#include <villas/format.hpp>

namespace villas {
namespace node {

class PrintHook : public Hook {

protected:
	Format::Ptr formatter;

	std::string prefix;
	std::string output_path;

	FILE *output;

public:
	PrintHook(Path *p, Node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		output(nullptr)
	{ }

	virtual
	void start()
	{
		assert(state == State::PREPARED ||
		       state == State::STOPPED);

		if (!output_path.empty()) {
			output = fopen(output_path.c_str(), "w+");
			if (!output)
				throw SystemError("Failed to open file");
		}

		formatter->start(signals);

		state = State::STARTED;
	}

	virtual
	void stop() {
		if (output)
			fclose(output);
	}

	virtual
	void parse(json_t *json)
	{
		const char *p = nullptr;
		const char *o = nullptr;
		int ret;
		json_error_t err;
		json_t *json_format = nullptr;

		assert(state == State::INITIALIZED ||
		       state == State::CHECKED ||
		       state == State::PARSED);

		Hook::parse(json);

		ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: o, s?: s }",
			"prefix", &p,
			"format", &json_format,
			"output", &o
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-print");

		if (p && o) {
			throw ConfigError(json, "node-config-hook-print", "Prefix and output settings are exclusive.");
		}

		if (p)
			prefix = p;

		if (o)
			output_path = o;

		/* Format */
		auto *fmt = json_format
				? FormatFactory::make(json_format)
				: FormatFactory::make("villas.human");

		formatter = Format::Ptr(fmt);
		if (!formatter)
			throw ConfigError(json_format, "node-config-hook-print-format", "Invalid format configuration");

		state = State::PARSED;
	}

	virtual
	Hook::Reason process(struct Sample *smp)
	{
		assert(state == State::STARTED);

		if (!output) {
			char buf[1024];
			size_t wbytes;

			formatter->sprint(buf, sizeof(buf), &wbytes, smp);

			if (wbytes > 0 && buf[wbytes-1] == '\n')
				buf[wbytes-1] = 0;

			if (node)
				logger->info("{}{} {}", prefix, *node, buf);
			else if (path)
				logger->info("{}{} {}", prefix, *path, buf);
		}
		else
			formatter->print(output, smp);

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "print";
static char d[] = "Print the message to stdout or a file";
static HookPlugin<PrintHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

