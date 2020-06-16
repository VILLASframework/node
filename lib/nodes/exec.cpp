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

#include <villas/node.h>
#include <villas/plugin.h>
#include <villas/node/config.h>
#include <villas/nodes/exec.hpp>
#include <villas/node/exceptions.hpp>

using namespace villas;
using namespace villas::utils;

int exec_parse(struct node *n, json_t *cfg)
{
	struct exec *e = (struct exec *) n->_vd;

	json_error_t err;
	int ret, flush = 1;

	json_t *json_exec;
	json_t *json_env = nullptr;

	const char *wd = nullptr;
	const char *format = "villas.human";
	int shell = -1;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: o, s?: s, s?: b, s?: o, s?: b, s?: s }",
		"exec", &json_exec,
		"format", &format,
		"flush", &flush,
		"environment", &json_env,
		"shell", &shell,
		"working_directory", &wd
	);
	if (ret)
		throw ConfigError(cfg, err, "node-config-node-exec");

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

	e->format = format_type_lookup(format);
	if (!e->format) {
		json_t *json_format = json_object_get(cfg, "format");
		throw ConfigError(json_format, "node-config-node-exec-format", "Invalid format: {)", format);
	}

	if (!(e->format->flags & (int) IOFlags::NEWLINES)) {
		json_t *json_format = json_object_get(cfg, "format");
		throw ConfigError(json_format, "node-config-node-exec-format", "Only line-delimited formats are currently supported");
	}

	return 0;
}

int exec_prepare(struct node *n)
{
	int ret;
	struct exec *e = (struct exec *) n->_vd;

	/* Initialize IO */
	ret = io_init(&e->io, e->format, &n->in.signals, (int) SampleFlags::HAS_ALL);
	if (ret)
		return ret;

	ret = io_check(&e->io);
	if (ret)
		return ret;

	/* Start subprocess */
	e->proc = std::make_unique<Popen>(e->command, e->arguments, e->environment, e->working_dir, e->shell);
	debug(2, "Started sub-process with pid=%d", e->proc->getPid());

	return 0;
}

int exec_init(struct node *n)
{
	struct exec *e = (struct exec *) n->_vd;

	new (&e->proc) std::unique_ptr<Popen>();
	new (&e->working_dir) std::string();
	new (&e->command) std::string();
	new (&e->arguments) Popen::arg_list();
	new (&e->environment) Popen::env_map();

	return 0;
}

int exec_destroy(struct node *n)
{
	int ret;
	struct exec *e = (struct exec *) n->_vd;

	if (e->io.state == State::INITIALIZED) {
		ret = io_destroy(&e->io);
		if (ret)
			return ret;
	}

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

int exec_start(struct node *n)
{
//	struct exec *e = (struct exec *) n->_vd;

	return 0;
}

int exec_stop(struct node *n)
{
	struct exec *e = (struct exec *) n->_vd;

	/* Stop subprocess */
	debug(2, "Killing sub-process with pid=%d", e->proc->getPid());
	e->proc->kill(SIGINT);

	debug(2, "Waiting for sub-process with pid=%d to terminate", e->proc->getPid());
	e->proc->close();

	/** @todo: Check exit code of subprocess? */
	return 0;
}

int exec_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct exec *e = (struct exec *) n->_vd;

	size_t rbytes;
	int avail;
	std::string line;

	std::getline(e->proc->cin(), line);

	avail = io_sscan(&e->io, line.c_str(), line.length(), &rbytes, smps, cnt);
	if (rbytes - 1 != line.length())
		return -1;

	return avail;
}

int exec_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct exec *e = (struct exec *) n->_vd;

	size_t wbytes;
	int ret;
	char *line = new char[1024];

	ret = io_sprint(&e->io, line, 1024, &wbytes, smps, cnt);
	if (ret < 0)
		return ret;

	e->proc->cout() << line;

	if (e->flush)
		e->proc->cout().flush();

	delete line;

	return cnt;
}

char * exec_print(struct node *n)
{
	struct exec *e = (struct exec *) n->_vd;
	char *buf = nullptr;

	strcatf(&buf, "format=%s, exec=%s, shell=%s, flush=%s, #environment=%zu, #arguments=%zu, working_dir=%s",
		format_type_name(e->format),
		e->command.c_str(),
		e->shell ? "yes" : "no",
		e->flush ? "yes" : "no",
		e->environment.size(),
		e->arguments.size(),
		e->working_dir.c_str()
	);

	return buf;
}

int exec_poll_fds(struct node *n, int fds[])
{
	struct exec *e = (struct exec *) n->_vd;

	fds[0] = e->proc->getFd();

	return 1;
}

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "exec";
	p.description		= "run subprocesses with stdin/stdout communication";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct exec);
	p.node.parse		= exec_parse;
	p.node.print		= exec_print;
	p.node.prepare		= exec_prepare;
	p.node.init		= exec_init;
	p.node.destroy		= exec_destroy;
	p.node.start		= exec_start;
	p.node.stop		= exec_stop;
	p.node.read		= exec_read;
	p.node.write		= exec_write;
	p.node.poll_fds		= exec_poll_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
