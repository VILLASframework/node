/** Node type: File
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

#include <unistd.h>
#include <cstring>
#include <cinttypes>
#include <libgen.h>
#include <sys/stat.h>
#include <cerrno>

#include <villas/node_compat.hpp>
#include <villas/nodes/file.hpp>
#include <villas/utils.hpp>
#include <villas/timing.hpp>
#include <villas/queue.h>
#include <villas/format.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static
char * file_format_name(const char *format, struct timespec *ts)
{
	struct tm tm;
	char *buf = new char[FILE_MAX_PATHLEN];
	if (!buf)
		throw MemoryAllocationError();

	/* Convert time */
	gmtime_r(&ts->tv_sec, &tm);

	strftime(buf, FILE_MAX_PATHLEN, format, &tm);

	return buf;
}

static
struct timespec file_calc_offset(const struct timespec *first, const struct timespec *epoch, enum file::EpochMode mode)
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

int villas::node::file_parse(NodeCompat *n, json_t *json)
{
	auto *f = n->getData<struct file>();

	int ret;
	json_error_t err;
	json_t *json_format = nullptr;

	const char *uri_tmpl = nullptr;
	const char *eof = nullptr;
	const char *epoch = nullptr;
	double epoch_flt = 0;

	ret = json_unpack_ex(json, &err, 0, "{ s: s, s?: o, s?: { s?: s, s?: F, s?: s, s?: F, s?: i, s?: i }, s?: { s?: b, s?: i } }",
		"uri", &uri_tmpl,
		"format", &json_format,
		"in",
			"eof", &eof,
			"rate", &f->rate,
			"epoch_mode", &epoch,
			"epoch", &epoch_flt,
			"buffer_size", &f->buffer_size_in,
			"skip", &f->skip_lines,
		"out",
			"flush", &f->flush,
			"buffer_size", &f->buffer_size_out
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-file");

	f->epoch = time_from_double(epoch_flt);
	f->uri_tmpl = uri_tmpl ? strdup(uri_tmpl) : nullptr;

	/* Format */
	if (f->formatter)
		delete f->formatter;
	f->formatter = json_format
			? FormatFactory::make(json_format)
			: FormatFactory::make("villas.human");
	if (!f->formatter)
		throw ConfigError(json_format, "node-config-node-file-format", "Invalid format configuration");

	if (eof) {
		if      (!strcmp(eof, "exit") || !strcmp(eof, "stop"))
			f->eof_mode = file::EOFBehaviour::STOP;
		else if (!strcmp(eof, "rewind"))
			f->eof_mode = file::EOFBehaviour::REWIND;
		else if (!strcmp(eof, "wait"))
			f->eof_mode = file::EOFBehaviour::SUSPEND;
		else
			throw RuntimeError("Invalid mode '{}' for 'eof' setting", eof);
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
			throw RuntimeError("Invalid value '{}' for setting 'epoch'", epoch);
	}

	return 0;
}

char * villas::node::file_print(NodeCompat *n)
{
	auto *f = n->getData<struct file>();
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

	strcatf(&buf, "uri=%s, out.flush=%s, in.skip=%d, in.eof=%s, in.epoch=%s, in.epoch=%.2f",
		f->uri ? f->uri : f->uri_tmpl,
		f->flush ? "yes" : "no",
		f->skip_lines,
		eof_str,
		epoch_str,
		time_to_double(&f->epoch)
	);

	if (f->rate)
		strcatf(&buf, ", in.rate=%.1f", f->rate);

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

int villas::node::file_start(NodeCompat *n)
{
	auto *f = n->getData<struct file>();

	struct timespec now = time_now();
	int ret;

	/* Prepare file name */
	if (f->uri)
		delete[] f->uri;

	f->uri = file_format_name(f->uri_tmpl, &now);

	/* Check if directory exists */
	struct stat sb;
	char *cpy = strdup(f->uri);
	char *dir = dirname(cpy);

	ret = stat(dir, &sb);
	if (ret) {
		if (errno == ENOENT || errno == ENOTDIR) {
			ret = mkdir(dir, 0644);
			if (ret)
				throw SystemError("Failed to create directory");
		}
		else if (errno != EISDIR)
			throw SystemError("Failed to stat");
	}
	else if (!S_ISDIR(sb.st_mode)) {
		ret = mkdir(dir, 0644);
		if (ret)
			throw SystemError("Failed to create directory");
	}

	free(cpy);

	f->formatter->start(n->getInputSignals(false));

	/* Open file */
	f->stream_out = fopen(f->uri, "w+");
	if (!f->stream_out)
		return -1;

	f->stream_in = fopen(f->uri, "r");
	if (!f->stream_in)
		return -1;

	if (f->buffer_size_in) {
		ret = setvbuf(f->stream_in, nullptr, _IOFBF, f->buffer_size_in);
		if (ret)
			return ret;
	}

	if (f->buffer_size_out) {
		ret = setvbuf(f->stream_out, nullptr, _IOFBF, f->buffer_size_out);
		if (ret)
			return ret;
	}

	/* Create timer */
	f->task.setRate(f->rate);

	/* Get timestamp of first line */
	if (f->epoch_mode != file::EpochMode::ORIGINAL) {
		rewind(f->stream_in);

		if (feof(f->stream_in)) {
			n->logger->warn("Empty file");
		}
		else {
			struct Sample smp;

			smp.capacity = 0;

			ret = f->formatter->scan(f->stream_in, &smp);
			if (ret == 1) {
				f->first = smp.ts.origin;
				f->offset = file_calc_offset(&f->first, &f->epoch, f->epoch_mode);
			}
			else
				n->logger->warn("Failed to read first timestamp");
		}
	}

	rewind(f->stream_in);

	/* Fast-forward */
	struct Sample *smp = sample_alloc_mem(n->getInputSignals(false)->size());
	for (unsigned i = 0; i < f->skip_lines; i++)
		f->formatter->scan(f->stream_in, smp);

	sample_free(smp);

	return 0;
}

int villas::node::file_stop(NodeCompat *n)
{
	auto *f = n->getData<struct file>();

	f->task.stop();

	fclose(f->stream_in);
	fclose(f->stream_out);

	return 0;
}

int villas::node::file_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	auto *f = n->getData<struct file>();
	int ret;
	uint64_t steps;

	assert(cnt == 1);

retry:	ret = f->formatter->scan(f->stream_in, smps, cnt);
	if (ret <= 0) {
		if (feof(f->stream_in)) {
			switch (f->eof_mode) {
				case file::EOFBehaviour::REWIND:
					n->logger->info("Rewind input file");

					f->offset = file_calc_offset(&f->first, &f->epoch, f->epoch_mode);
					rewind(f->stream_in);
					goto retry;

				case file::EOFBehaviour::SUSPEND:
					/* We wait 10ms before fetching again. */
					usleep(100000);

					/* Try to download more data if this is a remote file. */
					clearerr(f->stream_in);
					goto retry;

				case file::EOFBehaviour::STOP:
					n->logger->info("Reached end-of-file.");

					n->setState(State::STOPPING);

					return -1;

				default: { }
			}
		}
		else
			n->logger->warn("Failed to read messages: reason={}", ret);

		return 0;
	}

	/* We dont wait in FILE_EPOCH_ORIGINAL mode */
	if (f->epoch_mode == file::EpochMode::ORIGINAL)
		return cnt;

	if (f->rate) {
		steps = f->task.wait();

		smps[0]->ts.origin = time_now();
	}
	else {
		smps[0]->ts.origin = time_add(&smps[0]->ts.origin, &f->offset);

		f->task.setNext(&smps[0]->ts.origin);
		steps = f->task.wait();
	}

	/* Check for overruns */
	if      (steps == 0)
		throw SystemError("Failed to wait for timer");
	else if (steps != 1)
		n->logger->warn("Missed steps: {}", steps - 1);

	return cnt;
}

int villas::node::file_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int ret;
	auto *f = n->getData<struct file>();

	assert(cnt == 1);

	ret = f->formatter->print(f->stream_out, smps, cnt);
	if (ret < 0)
		return ret;

	if (f->flush)
		fflush(f->stream_out);

	return cnt;
}

int villas::node::file_poll_fds(NodeCompat *n, int fds[])
{
	auto *f = n->getData<struct file>();

	if (f->rate) {
		fds[0] = f->task.getFD();

		return 1;
	}
	else if (f->epoch_mode == file::EpochMode::ORIGINAL) {
		fds[0] = fileno(f->stream_in);

		return 1;
	}

	return -1; /** @todo not supported yet */
}

int villas::node::file_init(NodeCompat *n)
{
	auto *f = n->getData<struct file>();

	// We require a real-time clock here as we can sync against the
	// timestamps in the file.
	new (&f->task) Task(CLOCK_REALTIME);

	/* Default values */
	f->rate = 0;
	f->eof_mode = file::EOFBehaviour::STOP;
	f->epoch_mode = file::EpochMode::DIRECT;
	f->flush = 0;
	f->buffer_size_in = 0;
	f->buffer_size_out = 0;
	f->skip_lines = 0;

	f->formatter = nullptr;

	return 0;
}

int villas::node::file_destroy(NodeCompat *n)
{
	auto *f = n->getData<struct file>();

	f->task.~Task();

	if (f->uri)
		delete[] f->uri;

	if (f->formatter)
		delete f->formatter;

	return 0;
}

static NodeCompatType p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "file";
	p.description	= "support for file log / replay node type";
	p.vectorize	= 1;
	p.size		= sizeof(struct file);
	p.init		= file_init;
	p.destroy	= file_destroy;
	p.parse		= file_parse;
	p.print		= file_print;
	p.start		= file_start;
	p.stop		= file_stop;
	p.read		= file_read;
	p.write		= file_write;
	p.poll_fds	= file_poll_fds;

	static NodeCompatFactory ncp(&p);
}
