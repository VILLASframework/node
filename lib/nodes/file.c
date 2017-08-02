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

#include "nodes/file.h"
#include "utils.h"
#include "timing.h"
#include "queue.h"
#include "plugin.h"
#include "formats/villas.h"

int file_reverse(struct node *n)
{
	struct file *f = n->_vd;
	struct file_direction tmp;

	tmp = f->read;
	f->read = f->write;
	f->write = tmp;

	return 0;
}

static char * file_format_name(const char *format, struct timespec *ts)
{
	struct tm tm;
	char *buf = alloc(FILE_MAX_PATHLEN);

	/* Convert time */
	gmtime_r(&ts->tv_sec, &tm);

	strftime(buf, FILE_MAX_PATHLEN, format, &tm);

	return buf;
}

static AFILE * file_reopen(struct file_direction *dir)
{
	if (dir->handle)
		afclose(dir->handle);

	return afopen(dir->uri, dir->mode);
}

static int file_parse_direction(json_t *cfg, struct file *f, int d)
{
	struct file_direction *dir = (d == FILE_READ) ? &f->read : &f->write;
	int ret;
	json_error_t err;

	const char *format = NULL;
	const char *mode = NULL;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: s, s?: s }",
		"uri", &format,
		"mode", &mode
	);
	if (ret)
		jerror(&err, "Failed to ");

	if (format)
		dir->format = strdup(format);

	if (mode)
		dir->mode = strdup(mode);
	else
		dir->mode = strdup(d == FILE_READ ? "r" : "w+");

	return 0;
}

static struct timespec file_calc_read_offset(const struct timespec *first, const struct timespec *epoch, enum read_epoch_mode mode)
{
	/* Get current time */
	struct timespec now = time_now();
	struct timespec offset;

	/* Set read_offset depending on epoch_mode */
	switch (mode) {
		case FILE_EPOCH_DIRECT: /* read first value at now + epoch */
			offset = time_diff(first, &now);
			return time_add(&offset, epoch);

		case FILE_EPOCH_WAIT: /* read first value at now + first + epoch */
			offset = now;
			return time_add(&now, epoch);

		case FILE_EPOCH_RELATIVE: /* read first value at first + epoch */
			return *epoch;

		case FILE_EPOCH_ABSOLUTE: /* read first value at f->read_epoch */
			return time_diff(first, epoch);

		default:
			return (struct timespec) { 0 };
	}
}

int file_parse(struct node *n, json_t *cfg)
{
	struct file *f = n->_vd;

	json_t *cfg_in = NULL;
	json_t *cfg_out = NULL;

	int ret;
	json_error_t err;


	ret = json_unpack_ex(cfg, &err, 0, "{ s?: o, s?: o }",
		"in", &cfg_in,
		"out", &cfg_out
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (cfg_out) {
		if (file_parse_direction(cfg_out, f, FILE_WRITE))
			error("Failed to parse output file for node %s", node_name(n));

		f->flush = 0;

		ret = json_unpack_ex(cfg_out, &err, 0, "{ s?: b }", "flush", &f->flush);
		if (ret)
			jerror(&err, "Failed to parse configuration of node %s", node_name(n));
	}

	if (cfg_in) {
		const char *eof = NULL;
		const char *epoch_mode = NULL;
		double epoch_flt = 0;

		/* Default values */
		f->read_rate = 0;
		f->read_eof = FILE_EOF_EXIT;
		f->read_epoch_mode = FILE_EPOCH_DIRECT;

		if (file_parse_direction(cfg_in, f, FILE_READ))
			error("Failed to parse input file for node %s", node_name(n));

		ret = json_unpack_ex(cfg_in, &err, 0, "{ s?: s, s?: f, s?: s, s?: f }",
			"eof", &eof,
			"rate", &f->read_rate,
			"epoch_mode", &epoch_mode,
			"epoch", &epoch_flt
		);
		if (ret)
			jerror(&err, "Failed to parse configuration of node %s", node_name(n));

		f->read_epoch = time_from_double(epoch_flt);

		/* More read specific settings */
		if (eof) {
			if      (!strcmp(eof, "exit"))
				f->read_eof = FILE_EOF_EXIT;
			else if (!strcmp(eof, "rewind"))
				f->read_eof = FILE_EOF_REWIND;
			else if (!strcmp(eof, "wait"))
				f->read_eof = FILE_EOF_WAIT;
			else
				error("Invalid mode '%s' for 'eof' setting of node %s", eof, node_name(n));
		}

		if (epoch_mode) {
			if     (!strcmp(epoch_mode, "direct"))
				f->read_epoch_mode = FILE_EPOCH_DIRECT;
			else if (!strcmp(epoch_mode, "wait"))
				f->read_epoch_mode = FILE_EPOCH_WAIT;
			else if (!strcmp(epoch_mode, "relative"))
				f->read_epoch_mode = FILE_EPOCH_RELATIVE;
			else if (!strcmp(epoch_mode, "absolute"))
				f->read_epoch_mode = FILE_EPOCH_ABSOLUTE;
			else if (!strcmp(epoch_mode, "original"))
				f->read_epoch_mode = FILE_EPOCH_ORIGINAL;
			else
				error("Invalid value '%s' for setting 'epoch_mode' of node %s", epoch_mode, node_name(n));
		}
	}

	n->_vd = f;

	return 0;
}

char * file_print(struct node *n)
{
	struct file *f = n->_vd;
	char *buf = NULL;

	if (f->read.format) {
		const char *epoch_str = NULL;
		switch (f->read_epoch_mode) {
			case FILE_EPOCH_DIRECT:   epoch_str = "direct";   break;
			case FILE_EPOCH_WAIT:     epoch_str = "wait";     break;
			case FILE_EPOCH_RELATIVE: epoch_str = "relative"; break;
			case FILE_EPOCH_ABSOLUTE: epoch_str = "absolute"; break;
			case FILE_EPOCH_ORIGINAL: epoch_str = "original"; break;
		}

		const char *eof_str = NULL;
		switch (f->read_eof) {
			case FILE_EOF_EXIT:   eof_str = "exit";   break;
			case FILE_EOF_WAIT:   eof_str = "wait";   break;
			case FILE_EOF_REWIND: eof_str = "rewind"; break;
		}

		strcatf(&buf, "in=%s, mode=%s, eof=%s, epoch_mode=%s, epoch=%.2f",
			f->read.uri ? f->read.uri : f->read.format,
			f->read.mode,
			eof_str,
			epoch_str,
			time_to_double(&f->read_epoch)
		);

		if (f->read_rate)
			strcatf(&buf, ", rate=%.1f", f->read_rate);
	}

	if (f->write.format) {
		strcatf(&buf, ", out=%s, mode=%s",
			f->write.uri ? f->write.uri : f->write.format,
			f->write.mode
		);
	}

	if (f->read_first.tv_sec || f->read_first.tv_nsec)
		strcatf(&buf, ", first=%.2f", time_to_double(&f->read_first));

	if (f->read_offset.tv_sec || f->read_offset.tv_nsec)
		strcatf(&buf, ", offset=%.2f", time_to_double(&f->read_offset));

	if ((f->read_first.tv_sec || f->read_first.tv_nsec) &&
	    (f->read_offset.tv_sec || f->read_offset.tv_nsec)) {
		struct timespec eta, now = time_now();

		eta = time_add(&f->read_first, &f->read_offset);
		eta = time_diff(&now, &eta);

		if (eta.tv_sec || eta.tv_nsec)
			strcatf(&buf, ", eta=%.2f sec", time_to_double(&eta));
	}

	return buf;
}

int file_start(struct node *n)
{
	struct file *f = n->_vd;

	struct timespec now = time_now();
	int ret;

	if (f->read.format) {
		/* Prepare file name */
		f->read.uri = file_format_name(f->read.format, &now);

		/* Open file */
		f->read.handle = file_reopen(&f->read);
		if (!f->read.handle)
			serror("Failed to open file for reading: '%s'", f->read.uri);

		/* Create timer */
		f->read_timer = f->read_rate
			? timerfd_create_rate(f->read_rate)
			: timerfd_create(CLOCK_REALTIME, 0);
		if (f->read_timer < 0)
			serror("Failed to create timer");

		arewind(f->read.handle);

		/* Get timestamp of first line */
		if (f->read_epoch_mode != FILE_EPOCH_ORIGINAL) {
			struct sample s;
			struct sample *smps[] = { &s };
			s.capacity = 0;

			ret = io_format_villas_fscan(f->read.handle->file, smps, 1, NULL);
			if (ret < 0)
				error("Failed to read first timestamp of node %s", node_name(n));

			f->read_first = s.ts.origin;
			f->read_offset = file_calc_read_offset(&f->read_first, &f->read_epoch, f->read_epoch_mode);
			arewind(f->read.handle);
		}
	}

	if (f->write.format) {
		/* Prepare file name */
		f->write.uri   = file_format_name(f->write.format, &now);

		/* Open file */
		f->write.handle = file_reopen(&f->write);
		if (!f->write.handle)
			serror("Failed to open file for writing: '%s'", f->write.uri);
	}

	return 0;
}

int file_stop(struct node *n)
{
	struct file *f = n->_vd;

	if (f->read_timer)
		close(f->read_timer);
	if (f->read.handle)
		afclose(f->read.handle);
	if (f->write.handle)
		afclose(f->write.handle);

	free(f->read.uri);
	free(f->write.uri);

	return 0;
}

int file_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct file *f = n->_vd;
	int values, flags;
	uint64_t ex;

	assert(f->read.handle);
	assert(cnt == 1);

retry:	values = io_format_villas_fscan(f->read.handle->file, smps, 1, &flags); /* Get message and timestamp */
	if (values <= 0) {
		if (afeof(f->read.handle)) {
			switch (f->read_eof) {
				case FILE_EOF_REWIND:
					info("Rewind input file of node %s", node_name(n));

					f->read_offset = file_calc_read_offset(&f->read_first, &f->read_epoch, f->read_epoch_mode);
					arewind(f->read.handle);
					goto retry;

				case FILE_EOF_WAIT:
					usleep(10000); /* We wait 10ms before fetching again. */
					adownload(f->read.handle, 1);
					goto retry;

				case FILE_EOF_EXIT:
					info("Reached end-of-file of node %s", node_name(n));
					killme(SIGTERM);
					pause();

			}
		}
		else
			warn("Failed to read messages from node %s: reason=%d", node_name(n), values);

		return 0;
	}

	if (f->read_epoch_mode != FILE_EPOCH_ORIGINAL) {
		if (!f->read_rate || aftell(f->read.handle) == 0) {
			smps[0]->ts.origin = time_add(&smps[0]->ts.origin, &f->read_offset);

			ex = timerfd_wait_until(f->read_timer, &smps[0]->ts.origin);
		}
		else { /* Wait with fixed rate delay */
			ex = timerfd_wait(f->read_timer);

			smps[0]->ts.origin = time_now();
		}

		/* Check for overruns */
		if (ex == 0)
			serror("Failed to wait for timer");
		else if (ex != 1)
			warn("Overrun: %" PRIu64, ex - 1);
	}

	return 1;
}

int file_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct file *f = n->_vd;

	assert(f->write.handle);
	assert(cnt == 1);

	io_format_villas_fprint(f->write.handle->file, smps, cnt, IO_FORMAT_ALL & ~IO_FORMAT_OFFSET);

	if (f->flush)
		afflush(f->write.handle);

	return 1;
}

static struct plugin p = {
	.name		= "file",
	.description	= "support for file log / replay node type",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 1,
		.size		= sizeof(struct file),
		.reverse	= file_reverse,
		.parse		= file_parse,
		.print		= file_print,
		.start		= file_start,
		.stop		= file_stop,
		.read		= file_read,
		.write		= file_write
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
