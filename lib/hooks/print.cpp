/** Print hook.
 *
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
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include <string.h>

#include <villas/hook.hpp>
#include <villas/path.h>
#include <villas/sample.h>
#include <villas/io.h>
#include <villas/plugin.h>

namespace villas {
namespace node {

class PrintHook : public Hook {

protected:
	struct io io;
	struct format_type *format;

	char *prefix;
	char *uri;

public:
	PrintHook(struct path *p, struct node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		prefix(nullptr),
		uri(nullptr)
	{
		io.state = State::DESTROYED;

		format = format_type_lookup("villas.human");
	}

	virtual void start()
	{
		int ret;

		assert(state == State::PREPARED || state == State::STOPPED);

		ret = io_init(&io, format, &signals, (int) SampleFlags::HAS_ALL);
		if (ret)
			throw RuntimeError("Failed to initialze IO");

		ret = io_check(&io);
		if (ret)
			throw RuntimeError("Failed to check IO");

		ret = io_open(&io, uri);
		if (ret)
			throw RuntimeError("Failed to open IO");

		state = State::STARTED;
	}

	virtual void stop()
	{
		int ret;

		assert(state == State::STARTED);

		ret = io_close(&io);
		if (ret)
			throw RuntimeError("Failed to close IO");

		ret = io_destroy(&io);
		if (ret)
			throw RuntimeError("Failed to destroy IO");

		state = State::STOPPED;
	}

	virtual void parse(json_t *cfg)
	{
		const char *f = nullptr, *p = nullptr, *u = nullptr;
		int ret;
		json_error_t err;

		assert(state != State::STARTED);

		ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: s, s?: s }",
			"output", &u,
			"prefix", &p,
			"format", &f
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-print");

		if (p)
			prefix = strdup(p);

		if (u)
			uri = strdup(u);

		if (f) {
			format = format_type_lookup(f);
			if (!format)
				throw ConfigError(cfg, "node-config-hook-print-format", "Invalid IO format '{}'", f);
		}

		state = State::PARSED;
	}

	virtual Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);

		if (prefix)
			printf("%s", prefix);
		else if (node)
			printf("Node %s: ", node_name(node));
		else if (path)
			printf("Path %s: ", path_name(path));

		io_print(&io, &smp, 1);

		return Reason::OK;
	}

	virtual ~PrintHook()
	{
		if (uri)
			free(uri);

		if (prefix)
			free(prefix);
	}
};

/* Register hook */
static HookPlugin<PrintHook> p(
	"print",
	"Print the message to stdout",
	(int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH,
	99
);

} /* namespace node */
} /* namespace villas */

/** @} */

