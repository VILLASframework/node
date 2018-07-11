/** Node type: File
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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
#include <string.h>
#include <inttypes.h>

#include <villas/nodes/file.h>
#include <villas/utils.h>
#include <villas/timing.h>
#include <villas/queue.h>
#include <villas/plugin.h>
#include <villas/io.h>

static char * file_format_name(const char *format, struct timespec *ts)
{
	struct tm tm;
	char *buf = alloc(FILE_MAX_PATHLEN);

	/* Convert time */
	gmtime_r(&ts->tv_sec, &tm);

	strftime(buf, FILE_MAX_PATHLEN, format, &tm);

	return buf;
}

static struct timespec file_calc_offset(const struct timespec *first, const struct timespec *epoch, enum epoch_mode mode)
{
	/* Get current time */
	struct timespec now = time_now();
	struct timespec offset;

	/* Set offset depending on epoch_mode */
	switch (mode) {
		case FILE_EPOCH_DIRECT: /* read first value at now + epoch */
			offset = time_diff(first, &now);
			return time_add(&offset, epoch);

		case FILE_EPOCH_WAIT: /* read first value at now + first + epoch */
			offset = now;
			return time_add(&now, epoch);

		case FILE_EPOCH_RELATIVE: /* read first value at first + epoch */
			return *epoch;

		case FILE_EPOCH_ABSOLUTE: /* read first value at f->epoch */
			return time_diff(first, epoch);

		default:
			return (struct timespec) { 0 };
	}
}

int file_parse(struct node *n, json_t *cfg)
{
	struct file *f = n->_vd;

	int ret;
	json_error_t err;

	const char *uri_tmpl = NULL;
	const char *format = "villas.human";
	const char *eof = NULL;
	const char *epoch_mode = NULL;
	double epoch_flt = 0;

	/* Default values */
	f->rate = 0;
	f->eof = FILE_EOF_EXIT;
	f->epoch_mode = FILE_EPOCH_DIRECT;
	f->flush = 0;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: s, s?: b, s?: s, s?: F, s?: s, s?: F, s?: s }",
		"uri", &uri_tmpl,
		"flush", &f->flush,
		"eof", &eof,
		"rate", &f->rate,
		"epoch_mode", &epoch_mode,
		"epoch", &epoch_flt,
		"format", &format
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	f->epoch = time_from_double(epoch_flt);
	f->uri_tmpl = uri_tmpl ? strdup(uri_tmpl) : NULL;

	f->format = format_type_lookup(format);
	if (!f->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	if (eof) {
		if      (!strcmp(eof, "exit"))
			f->eof = FILE_EOF_EXIT;
		else if (!strcmp(eof, "rewind"))
			f->eof = FILE_EOF_REWIND;
		else if (!strcmp(eof, "wait"))
			f->eof = FILE_EOF_WAIT;
		else
			error("Invalid mode '%s' for 'eof' setting of node %s", eof, node_name(n));
	}

	if (epoch_mode) {
		if     (!strcmp(epoch_mode, "direct"))
			f->epoch_mode = FILE_EPOCH_DIRECT;
		else if (!strcmp(epoch_mode, "wait"))
			f->epoch_mode = FILE_EPOCH_WAIT;
		else if (!strcmp(epoch_mode, "relative"))
			f->epoch_mode = FILE_EPOCH_RELATIVE;
		else if (!strcmp(epoch_mode, "absolute"))
			f->epoch_mode = FILE_EPOCH_ABSOLUTE;
		else if (!strcmp(epoch_mode, "original"))
			f->epoch_mode = FILE_EPOCH_ORIGINAL;
		else
			error("Invalid value '%s' for setting 'epoch_mode' of node %s", epoch_mode, node_name(n));
	}

	n->_vd = f;

	return 0;
}

char * file_print(struct node *n)
{
	struct file *f = (struct file *) n->_vd;
	char *buf = NULL;

	const char *epoch_str = NULL;
	const char *eof_str = NULL;

	switch (f->epoch_mode) {
		case FILE_EPOCH_DIRECT:		epoch_str = "direct";	break;
		case FILE_EPOCH_WAIT:		epoch_str = "wait";	break;
		case FILE_EPOCH_RELATIVE:	epoch_str = "relative";	break;
		case FILE_EPOCH_ABSOLUTE:	epoch_str = "absolute";	break;
		case FILE_EPOCH_ORIGINAL:	epoch_str = "original";	break;
	}

	switch (f->eof) {
		case FILE_EOF_EXIT:		eof_str = "exit";	break;
		case FILE_EOF_WAIT:		eof_str = "wait";	break;
		case FILE_EOF_REWIND:		eof_str = "rewind";	break;
	}

	strcatf(&buf, "uri=%s, format=%s, flush=%s, eof=%s, epoch_mode=%s, epoch=%.2f",
		f->uri ? f->uri : f->uri_tmpl,
		plugin_name(f->format),
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

	/* Open file */
	flags = SAMPLE_HAS_ALL;
	if (f->flush)
		flags |= IO_FLUSH;

	ret = io_init(&f->io, f->format, n, flags);
	if (ret)
		return ret;

	ret = io_open(&f->io, f->uri);
	if (ret)
		return ret;

	/* Create timer */
	ret = task_init(&f->task, f->rate, CLOCK_REALTIME);
	if (ret)
		serror("Failed to create timer");

	/* Get timestamp of first line */
	if (f->epoch_mode != FILE_EPOCH_ORIGINAL) {
		io_rewind(&f->io);

		struct sample s = { .capacity = 0 };
		struct sample *smps[] = { &s };

		if (io_eof(&f->io)) {
			warn("Empty file");
		}
		else {
			ret = io_scan(&f->io, smps, 1);
			if (ret == 1) {
				f->first = s.ts.origin;
				f->offset = file_calc_offset(&f->first, &f->epoch, f->epoch_mode);
			}
			else
				warn("Failed to read first timestamp of node %s", node_name(n));
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

	free(f->uri);

	return 0;
}

int file_destroy(struct node *n)
{
	int ret;
	struct file *f = (struct file *) n->_vd;

	ret = io_destroy(&f->io);
	if (ret)
		return ret;

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
			switch (f->eof) {
				case FILE_EOF_REWIND:
					info("Rewind input file of node %s", node_name(n));

					f->offset = file_calc_offset(&f->first, &f->epoch, f->epoch_mode);
					io_rewind(&f->io);
					goto retry;

				case FILE_EOF_WAIT:
					/* We wait 10ms before fetching again. */
					usleep(100000);

					/* Try to download more data if this is a remote file. */
					if (f->io.mode == IO_MODE_ADVIO)
						adownload(f->io.input.stream.adv, 1);

					goto retry;

				case FILE_EOF_EXIT:
					info("Reached end-of-file of node %s", node_name(n));

					killme(SIGTERM);
					pause();
			}
		}
		else
			warn("Failed to read messages from node %s: reason=%d", node_name(n), ret);

		return 0;
	}

	/* We dont wait in FILE_EPOCH_ORIGINAL mode */
	if (f->epoch_mode == FILE_EPOCH_ORIGINAL)
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
		warn("Missed steps: %" PRIu64, steps - 1);

	return cnt;
}

int file_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct file *f = (struct file *) n->_vd;

	assert(cnt == 1);

	io_print(&f->io, smps, cnt);

	return cnt;
}

int file_fd(struct node *n)
{
	struct file *f = (struct file *) n->_vd;

	if (f->rate)
		return task_fd(&f->task);
	else {
		if (f->epoch_mode == FILE_EPOCH_ORIGINAL)
			return io_fd(&f->io);
		else
			return -1; /** @todo not supported yet */
	}
}

static struct plugin p = {
	.name		= "file",
	.description	= "support for file log / replay node type",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 1,
		.size		= sizeof(struct file),
		.parse		= file_parse,
		.print		= file_print,
		.start		= file_start,
		.stop		= file_stop,
		.destroy	= file_destroy,
		.read		= file_read,
		.write		= file_write,
		.fd		= file_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
