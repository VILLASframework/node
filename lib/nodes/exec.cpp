/** Node-type for subprocess node-types.
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

#include <string>

#include <villas/node/config.h>
#include <villas/node.h>
#include <villas/nodes/exec.hpp>
#include <villas/utils.hpp>
#include <villas/node/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

int exec_parse(struct vnode *n, json_t *json)
{
	struct exec *e = (struct exec *) n->_vd;

	json_error_t err;
	int ret, flush = 1;

	json_t *json_exec;
	json_t *json_env = nullptr;
	json_t *json_format = nullptr;

	const char *wd = nullptr;
	int shell = -1;

	ret = json_unpack_ex(json, &err, 0, "{ s: o, s?: o, s?: b, s?: o, s?: b, s?: s }",
		"exec", &json_exec,
		"format", &json_format,
		"flush", &flush,
		"environment", &json_env,
		"shell", &shell,
		"working_directory", &wd
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-exec");

	e->flush = flush;
	e->shell = shell < 0 ? json_is_string(json_exec) : shell;

	e->arguments.clear();
	e->environment.clear();

	if (json_is_string(json_exec)) {
		if (shell == 0)
			throw ConfigError(json_exec, "node-config-node-exec-shell", "The exec setting must be an array if shell mode is disabled.");

		e->command = json_string_value(json_exec);
	}
	else if (json_is_array(json_exec)) {
		if (shell == 1)
			throw ConfigError(json_exec, "node-config-node-exec-shell", "The exec setting must be a string if shell mode is enabled.");

		if (json_array_size(json_exec) < 1)
			throw ConfigError(json_exec, "node-config-node-exec-exec", "At least one argument must be given");

		size_t i;
		json_t *json_arg;
		json_array_foreach(json_exec, i, json_arg) {
			if (!json_is_string(json_arg))
				throw ConfigError(json_arg, "node-config-node-exec-exec", "All arguments must be of string type");

			if (i == 0)
				e->command = json_string_value(json_arg);

			e->arguments.push_back(json_string_value(json_arg));
		}
	}

	if (json_env) {
		/* obj is a JSON object */
		const char *key;
		json_t *json_value;

		json_object_foreach(json_env, key, json_value) {
			if (!json_is_string(json_value))
				throw ConfigError(json_value, "node-config-node-exec-environment", "Environment variables must be of string type");

			e->environment[key] = json_string_value(json_value);
		}
	}

	/* Format */
	e->formatter = json_format
			? FormatFactory::make(json_format)
			: FormatFactory::make("villas.human");
	if (!e->formatter)
		throw ConfigError(json_format, "node-config-node-exec-format", "Invalid format configuration");

	// if (!(e->format->flags & (int) Flags::NEWLINES))
	// 	throw ConfigError(json_format, "node-config-node-exec-format", "Only line-delimited formats are currently supported");

	return 0;
}

int exec_prepare(struct vnode *n)
{
	struct exec *e = (struct exec *) n->_vd;

	/* Initialize IO */
	e->formatter->start(&n->in.signals);

	/* Start subprocess */
	e->proc = std::make_unique<Popen>(e->command, e->arguments, e->environment, e->working_dir, e->shell);
	n->logger->debug("Started sub-process with pid={}", e->proc->getPid());

	return 0;
}

int exec_init(struct vnode *n)
{
	struct exec *e = (struct exec *) n->_vd;

	new (&e->proc) std::unique_ptr<Popen>();
	new (&e->working_dir) std::string();
	new (&e->command) std::string();
	new (&e->arguments) Popen::arg_list();
	new (&e->environment) Popen::env_map();

	return 0;
}

int exec_destroy(struct vnode *n)
{
	struct exec *e = (struct exec *) n->_vd;

	delete e->formatter;

	using uptr = std::unique_ptr<Popen>;
	using str = std::string;
	using al = Popen::arg_list;
	using em = Popen::env_map;

	e->proc.~uptr();
	e->working_dir.~str();
	e->command.~str();
	e->arguments.~al();
	e->environment.~em();

	return 0;
}

int exec_stop(struct vnode *n)
{
	struct exec *e = (struct exec *) n->_vd;

	/* Stop subprocess */
	n->logger->debug("Killing sub-process with pid={}", e->proc->getPid());
	e->proc->kill(SIGINT);

	n->logger->debug("Waiting for sub-process with pid={} to terminate", e->proc->getPid());
	e->proc->close();

	/** @todo: Check exit code of subprocess? */
	return 0;
}

int exec_read(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	struct exec *e = (struct exec *) n->_vd;

	size_t rbytes;
	int avail;
	std::string line;

	std::getline(e->proc->cin(), line);

	avail = e->formatter->sscan(line.c_str(), line.length(), &rbytes, smps, cnt);
	if (rbytes - 1 != line.length())
		return -1;

	return avail;
}

int exec_write(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	struct exec *e = (struct exec *) n->_vd;

	size_t wbytes;
	int ret;
	char *line = new char[1024];
	if (!line)
		throw MemoryAllocationError();

	ret = e->formatter->sprint(line, 1024, &wbytes, smps, cnt);
	if (ret < 0)
		return ret;

	e->proc->cout() << line;

	if (e->flush)
		e->proc->cout().flush();

	delete[] line;

	return cnt;
}

char * exec_print(struct vnode *n)
{
	struct exec *e = (struct exec *) n->_vd;
	char *buf = nullptr;

	strcatf(&buf, "exec=%s, shell=%s, flush=%s, #environment=%zu, #arguments=%zu, working_dir=%s",
		e->command.c_str(),
		e->shell ? "yes" : "no",
		e->flush ? "yes" : "no",
		e->environment.size(),
		e->arguments.size(),
		e->working_dir.c_str()
	);

	return buf;
}

int exec_poll_fds(struct vnode *n, int fds[])
{
	struct exec *e = (struct exec *) n->_vd;

	fds[0] = e->proc->getFd();

	return 1;
}

static struct vnode_type p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "exec";
	p.description	= "run subprocesses with stdin/stdout communication";
	p.vectorize	= 0;
	p.size		= sizeof(struct exec);
	p.parse		= exec_parse;
	p.print		= exec_print;
	p.prepare	= exec_prepare;
	p.init		= exec_init;
	p.destroy	= exec_destroy;
	p.stop		= exec_stop;
	p.read		= exec_read;
	p.write		= exec_write;
	p.poll_fds	= exec_poll_fds;

	if (!node_types)
		node_types = new NodeTypeList();

	node_types->push_back(&p);
}
