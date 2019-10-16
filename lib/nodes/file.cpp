/** Node type: File
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

#include <unistd.h>
#include <cstring>
#include <cinttypes>
#include <libgen.h>
#include <sys/stat.h>
#include <cerrno>

#include <villas/nodes/file.hpp>
#include <villas/utils.hpp>
#include <villas/timing.h>
#include <villas/queue.h>
#include <villas/plugin.h>
#include <villas/io.h>

using namespace villas::utils;

static char * file_format_name(const char *format, struct timespec *ts)
{
	struct tm tm;
	char *buf = (char *) alloc(FILE_MAX_PATHLEN);

	/* Convert time */
	gmtime_r(&ts->tv_sec, &tm);

	strftime(buf, FILE_MAX_PATHLEN, format, &tm);

	return buf;
}

static struct timespec file_calc_offset(const struct timespec *first, const struct timespec *epoch, enum file::EpochMode mode)
{
	/* Get current time */
	struct timespec now = time_now();
	struct timespec offset;

	/* Set offset depending on epoch */
	switch (mode) {
		case file::EpochMode::DIRECT: /* read first value at now + epoch */
			offset = time_diff(first, &now);
			return time_add(&offset, epoch);

		case file::EpochMode::WAIT: /* read first value at now + first + epoch */
			offset = now;
			return time_add(&now, epoch);

		case file::EpochMode::RELATIVE: /* read first value at first + epoch */
			return *epoch;

		case file::EpochMode::ABSOLUTE: /* read first value at f->epoch */
			return time_diff(first, epoch);

		default:
			return (struct timespec) { 0 };
	}
}

int file_parse(struct node *n, json_t *cfg)
{
	struct file *f = (struct file *) n->_vd;

	int ret;
	json_error_t err;

	const char *uri_tmpl = nullptr;
	const char *format = "villas.human";
	const char *eof = nullptr;
	const char *epoch = nullptr;
	double epoch_flt = 0;

	/* Default values */
	f->rate = 0;
	f->eof_mode = file::EOFBehaviour::STOP;
	f->epoch_mode = file::EpochMode::DIRECT;
	f->flush = 0;
	f->buffer_size_in = 0;
	f->buffer_size_out = 0;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: s, s?: s, s?: { s?: s, s?: F, s?: s, s?: F, s?: i }, s?: { s?: b, s?: i } }",
		"uri", &uri_tmpl,
		"format", &format,
		"in",
			"eof", &eof,
			"rate", &f->rate,
			"epoch_mode", &epoch,
			"epoch", &epoch_flt,
			"buffer_size", &f->buffer_size_in,
		"out",
			"flush", &f->flush,
			"buffer_size", &f->buffer_size_out
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	f->epoch = time_from_double(epoch_flt);
	f->uri_tmpl = uri_tmpl ? strdup(uri_tmpl) : nullptr;

	f->format = format_type_lookup(format);
	if (!f->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	if (eof) {
		if      (!strcmp(eof, "exit") || !strcmp(eof, "stop"))
			f->eof_mode = file::EOFBehaviour::STOP;
		else if (!strcmp(eof, "rewind"))
			f->eof_mode = file::EOFBehaviour::REWIND;
		else if (!strcmp(eof, "wait"))
			f->eof_mode = file::EOFBehaviour::SUSPEND;
		else
			error("Invalid mode '%s' for 'eof' setting of node %s", eof, node_name(n));
	}

	if (epoch) {
		if     (!strcmp(epoch, "direct"))
			f->epoch_mode = file::EpochMode::DIRECT;
		else if (!strcmp(epoch, "wait"))
			f->epoch_mode = file::EpochMode::WAIT;
		else if (!strcmp(epoch, "relative"))
			f->epoch_mode = file::EpochMode::RELATIVE;
		else if (!strcmp(epoch, "absolute"))
			f->epoch_mode = file::EpochMode::ABSOLUTE;
		else if (!strcmp(epoch, "original"))
			f->epoch_mode = file::EpochMode::ORIGINAL;
		else
			error("Invalid value '%s' for setting 'epoch' of node %s", epoch, node_name(n));
	}

	n->_vd = f;

	return 0;
}

char * file_print(struct node *n)
{
	struct file *f = (struct file *) n->_vd;
	char *buf = nullptr;

	const char *epoch_str = nullptr;
	const char *eof_str = nullptr;

	switch (f->epoch_mode) {
		case file::EpochMode::DIRECT:
			epoch_str = "direct";
			break;

		case file::EpochMode::WAIT:
			epoch_str = "wait";
			break;

		case file::EpochMode::RELATIVE:
			epoch_str = "relative";
			break;

		case file::EpochMode::ABSOLUTE:
			epoch_str = "absolute";
			break;

		case file::EpochMode::ORIGINAL:
			epoch_str = "original";
			break;

		default:
			epoch_str = "";
			break;
	}

	switch (f->eof_mode) {
		case file::EOFBehaviour::STOP:
			eof_str = "stop";
			break;

		case file::EOFBehaviour::SUSPEND:
			eof_str = "wait";
			break;

		case file::EOFBehaviour::REWIND:
			eof_str = "rewind";
			break;

		default:
			eof_str = "";
			break;
	}

	strcatf(&buf, "uri=%s, format=%s, flush=%s, eof=%s, epoch=%s, epoch=%.2f",
		f->uri ? f->uri : f->uri_tmpl,
		format_type_name(f->format),
		f->flush ? "yes" : "no",
		eof_str,
		epoch_str,
		time_to_double(&f->epoch)
	);

	if (f->rate)
		strcatf(&buf, ", rate=%.1f", f->rate);

	if (f->first.tv_sec || f->first.tv_nsec)
		strcatf(&buf, ", first=%.2f", time_to_double(&f->first));

	if (f->offset.tv_sec || f->offset.tv_nsec)
		strcatf(&buf, ", offset=%.2f", time_to_double(&f->offset));

	if ((f->first.tv_sec || f->first.tv_nsec) &&
	    (f->offset.tv_sec || f->offset.tv_nsec)) {
		struct timespec eta, now = time_now();

		eta = time_add(&f->first, &f->offset);
		eta = time_diff(&now, &eta);

		if (eta.tv_sec || eta.tv_nsec)
			strcatf(&buf, ", eta=%.2f sec", time_to_double(&eta));
	}

	return buf;
}

int file_start(struct node *n)
{
	struct file *f = (struct file *) n->_vd;

	struct timespec now = time_now();
	int ret, flags;

	/* Prepare file name */
	f->uri = file_format_name(f->uri_tmpl, &now);

	/* Check if local directory exists */
	if (aislocal(f->uri)) {
		struct stat sb;
		char *cpy = strdup(f->uri);
		char *dir = dirname(cpy);

		ret = stat(dir, &sb);
		if (ret) {
			if (errno == ENOENT || errno == ENOTDIR) {
				ret = mkdir(dir, 0644);
				if (ret)
					serror("Failed to create directory");
			}
			else if (errno != EISDIR)
				serror("Failed to stat");
		}
		else if (!S_ISDIR(sb.st_mode)) {
			ret = mkdir(dir, 0644);
			if (ret)
				serror("Failed to create directory");
		}

		free(cpy);
	}

	/* Open file */
	flags = (int) SampleFlags::HAS_ALL;
	if (f->flush)
		flags |= (int) IOFlags::FLUSH;

	ret = io_init(&f->io, f->format, &n->in.signals, flags);
	if (ret)
		return ret;

	ret = io_check(&f->io);
	if (ret)
		return ret;

	ret = io_open(&f->io, f->uri);
	if (ret)
		return ret;

	if (f->buffer_size_in) {
		ret = setvbuf(f->io.in.stream.std, nullptr, _IOFBF, f->buffer_size_in);
		if (ret)
			return ret;
	}

	if (f->buffer_size_out) {
		ret = setvbuf(f->io.out.stream.std, nullptr, _IOFBF, f->buffer_size_out);
		if (ret)
			return ret;
	}

	/* Create timer */
	ret = task_init(&f->task, f->rate, CLOCK_REALTIME);
	if (ret)
		serror("Failed to create timer");

	/* Get timestamp of first line */
	if (f->epoch_mode != file::EpochMode::ORIGINAL) {
		io_rewind(&f->io);

		if (io_eof(&f->io)) {
			warning("Empty file");
		}
		else {
			struct sample s;
			struct sample *smps[] = { &s };

			s.capacity = 0;

			ret = io_scan(&f->io, smps, 1);
			if (ret == 1) {
				f->first = s.ts.origin;
				f->offset = file_calc_offset(&f->first, &f->epoch, f->epoch_mode);
			}
			else
				warning("Failed to read first timestamp of node %s", node_name(n));
		}
	}

	io_rewind(&f->io);

	return 0;
}

int file_stop(struct node *n)
{
	int ret;
	struct file *f = (struct file *) n->_vd;

	ret = task_destroy(&f->task);
	if (ret)
		return ret;

	ret = io_close(&f->io);
	if (ret)
		return ret;

	ret = io_destroy(&f->io);
	if (ret)
		return ret;

	free(f->uri);

	return 0;
}

int file_restart(struct node *n)
{
	struct file *f = (struct file *) n->_vd;

	f->offset = file_calc_offset(&f->first, &f->epoch, f->epoch_mode);
	io_rewind(&f->io);

	return 0;
}

int file_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct file *f = (struct file *) n->_vd;
	int ret;
	uint64_t steps;

	assert(cnt == 1);

retry:	ret = io_scan(&f->io, smps, cnt);
	if (ret <= 0) {
		if (io_eof(&f->io)) {
			switch (f->eof_mode) {
				case file::EOFBehaviour::REWIND:
					info("Rewind input file of node %s", node_name(n));

					f->offset = file_calc_offset(&f->first, &f->epoch, f->epoch_mode);
					io_rewind(&f->io);
					goto retry;

				case file::EOFBehaviour::SUSPEND:
					/* We wait 10ms before fetching again. */
					usleep(100000);

					/* Try to download more data if this is a remote file. */
					switch (f->io.mode) {
						case IOMode::ADVIO:
							adownload(f->io.in.stream.adv, 1);
							break;

						case IOMode::STDIO:
							clearerr(f->io.in.stream.std);
							break;

						case IOMode::CUSTOM:
							break;
					}

					goto retry;

				case file::EOFBehaviour::STOP:
					info("Reached end-of-file.");

					n->state = State::STOPPING;

					return -1;

				default: { }
			}
		}
		else
			warning("Failed to read messages from node %s: reason=%d", node_name(n), ret);

		return 0;
	}

	/* We dont wait in FILE_EPOCH_ORIGINAL mode */
	if (f->epoch_mode == file::EpochMode::ORIGINAL)
		return cnt;

	if (f->rate) {
		steps = task_wait(&f->task);

		smps[0]->ts.origin = time_now();
	}
	else {
		smps[0]->ts.origin = time_add(&smps[0]->ts.origin, &f->offset);

		task_set_next(&f->task, &smps[0]->ts.origin);
		steps = task_wait(&f->task);
	}

	/* Check for overruns */
	if      (steps == 0)
		serror("Failed to wait for timer");
	else if (steps != 1)
		warning("Missed steps: %" PRIu64, steps - 1);

	return cnt;
}

int file_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct file *f = (struct file *) n->_vd;

	assert(cnt == 1);

	ret = io_print(&f->io, smps, cnt);
	if (ret < 0)
		return ret;

	return cnt;
}

int file_poll_fds(struct node *n, int fds[])
{
	struct file *f = (struct file *) n->_vd;

	if (f->rate) {
		fds[0] = task_fd(&f->task);

		return 1;
	}
	else if (f->epoch_mode == file::EpochMode::ORIGINAL) {
		fds[0] = io_fd(&f->io);

		return 1;
	}

	return -1; /** @todo not supported yet */
}

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	if (plugins.state == State::DESTROYED)
		vlist_init(&plugins);

	p.name			= "file";
	p.description		= "support for file log / replay node type";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 1;
	p.node.size		= sizeof(struct file);
	p.node.parse		= file_parse;
	p.node.print		= file_print;
	p.node.start		= file_start;
	p.node.restart		= file_restart;
	p.node.stop		= file_stop;
	p.node.read		= file_read;
	p.node.write		= file_write;
	p.node.poll_fds		= file_poll_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	if (plugins.state != State::DESTROYED)
		vlist_remove_all(&plugins, &p);
}
