/** Node type: File
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <unistd.h>
#include <string.h>

#include "nodes/file.h"
#include "utils.h"
#include "timing.h"
#include "queue.h"
#include "plugin.h"

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

static FILE * file_reopen(struct file_direction *dir)
{
	char buf[FILE_MAX_PATHLEN];
	const char *path = buf;

	/* Append chunk number to filename */
	if (dir->chunk >= 0)
		snprintf(buf, FILE_MAX_PATHLEN, "%s_%03u", dir->path, dir->chunk);
	else
		path = dir->path;

	if (dir->handle)
		fclose(dir->handle);

	return fopen(path, dir->mode);
}

static int file_parse_direction(config_setting_t *cfg, struct file *f, int d)
{
	struct file_direction *dir = (d == FILE_READ) ? &f->read : &f->write;

	if (!config_setting_lookup_string(cfg, "path", &dir->fmt))
		return -1;

	if (!config_setting_lookup_string(cfg, "mode", &dir->mode))
		dir->mode = (d == FILE_READ) ? "r" : "w+";

	return 0;
}

int file_parse(struct node *n, config_setting_t *cfg)
{
	struct file *f = n->_vd;
	
	config_setting_t *cfg_in, *cfg_out;
	
	cfg_out = config_setting_get_member(cfg, "out"); 
	if (cfg_out) {
		if (file_parse_direction(cfg_out, f, FILE_WRITE))
			cerror(cfg_out, "Failed to parse output file for node %s", node_name(n));

		/* More write specific settings */
		if (config_setting_lookup_int(cfg_out, "split", &f->write.split))
			f->write.split <<= 20; /* in MiB */
		else
			f->write.split = -1; /* Save all samples in a single file */
	}

	cfg_in = config_setting_get_member(cfg, "in");
	if (cfg_in) {
		if (file_parse_direction(cfg_in, f, FILE_READ))
			cerror(cfg_in, "Failed to parse input file for node %s", node_name(n));

		/* More read specific settings */
		if (!config_setting_lookup_bool(cfg_in, "splitted", &f->read.split))
			f->read.split = 0; /* Input files are suffixed with split indizes (.000, .001) */
		if (!config_setting_lookup_float(cfg_in, "rate", &f->read_rate))
			f->read_rate = 0; /* Disable fixed rate sending. Using timestamps of file instead */
		
		double epoch_flt;
		if (!config_setting_lookup_float(cfg_in, "epoch", &epoch_flt))
			epoch_flt = 0;
	
		f->read_epoch = time_from_double(epoch_flt);

		const char *epoch_mode;
		if (!config_setting_lookup_string(cfg_in, "epoch_mode", &epoch_mode))
			epoch_mode = "direct";

		if (!strcmp(epoch_mode, "direct"))
			f->read_epoch_mode = EPOCH_DIRECT;
		else if (!strcmp(epoch_mode, "wait"))
			f->read_epoch_mode = EPOCH_WAIT;
		else if (!strcmp(epoch_mode, "relative"))
			f->read_epoch_mode = EPOCH_RELATIVE;
		else if (!strcmp(epoch_mode, "absolute"))
			f->read_epoch_mode = EPOCH_ABSOLUTE;
		else
			cerror(cfg_in, "Invalid value '%s' for setting 'epoch_mode'", epoch_mode);
	}

	n->_vd = f;

	return 0;
}

char * file_print(struct node *n)
{
	struct file *f = n->_vd;
	char *buf = NULL;
	
	if (f->read.fmt) {
		const char *epoch_str = NULL;
		switch (f->read_epoch_mode) {
			case EPOCH_DIRECT:	epoch_str = "direct"; break;
			case EPOCH_WAIT:	epoch_str = "wait"; break;
			case EPOCH_RELATIVE:	epoch_str = "relative"; break;
			case EPOCH_ABSOLUTE:	epoch_str = "absolute"; break;
		}
		
		strcatf(&buf, "in=%s, epoch_mode=%s, epoch=%.2f, ",
			f->read.path ? f->read.path : f->read.fmt,
			epoch_str,
			time_to_double(&f->read_epoch)
		);
			
		if (f->read_rate)
			strcatf(&buf, "rate=%.1f, ", f->read_rate);
	}
	
	if (f->write.fmt) {
		strcatf(&buf, "out=%s, mode=%s, ",
			f->write.path ? f->write.path : f->write.fmt,
			f->write.mode
		);
	}
	
	if (f->read_first.tv_sec || f->read_first.tv_nsec)
		strcatf(&buf, "first=%.2f, ", time_to_double(&f->read_first));
	
	if (f->read_offset.tv_sec || f->read_offset.tv_nsec)
		strcatf(&buf, "offset=%.2f, ", time_to_double(&f->read_offset));
	
	if ((f->read_first.tv_sec || f->read_first.tv_nsec) &&
	    (f->read_offset.tv_sec || f->read_offset.tv_nsec)) {
		struct timespec eta, now = time_now();

		eta = time_add(&f->read_first, &f->read_offset);
		eta = time_diff(&now, &eta);

		if (eta.tv_sec || eta.tv_nsec)
		strcatf(&buf, "eta=%.2f sec, ", time_to_double(&eta));
	}
	
	if (strlen(buf) > 2)
		buf[strlen(buf) - 2] = 0;

	return buf;
}

int file_open(struct node *n)
{
	struct file *f = n->_vd;
	
	struct timespec now = time_now();

	if (f->read.fmt) {
		/* Prepare file name */
		f->read.chunk = f->read.split ? 0 : -1;
		f->read.path = file_format_name(f->read.fmt, &now);
		
		/* Open file */
		f->read.handle = file_reopen(&f->read);
		if (!f->read.handle)
			serror("Failed to open file for reading: '%s'", f->read.path);

		/* Create timer */
		f->read_timer = (f->read_rate)
			? timerfd_create_rate(f->read_rate)
			: timerfd_create(CLOCK_REALTIME, 0);
		if (f->read_timer < 0)
			serror("Failed to create timer");
		
		/* Get current time */
		struct timespec now = time_now();

		/* Get timestamp of first line */
		struct sample s;
		int ret = sample_fscan(f->read.handle, &s, NULL); rewind(f->read.handle);
		if (ret < 0)
			error("Failed to read first timestamp of node %s", node_name(n));
		
		f->read_first = s.ts.origin;

		/* Set read_offset depending on epoch_mode */
		switch (f->read_epoch_mode) {
			case EPOCH_DIRECT: /* read first value at now + epoch */
				f->read_offset = time_diff(&f->read_first, &now);
				f->read_offset = time_add(&f->read_offset, &f->read_epoch);
				break;

			case EPOCH_WAIT: /* read first value at now + first + epoch */
				f->read_offset = now;
				f->read_offset = time_add(&f->read_offset, &f->read_epoch);
				break;
		
			case EPOCH_RELATIVE: /* read first value at first + epoch */
				f->read_offset = f->read_epoch;
				break;
		
			case EPOCH_ABSOLUTE: /* read first value at f->read_epoch */
				f->read_offset = time_diff(&f->read_first, &f->read_epoch);
				break;
		}
	}

	if (f->write.fmt) {
		/* Prepare file name */
		f->write.chunk  = f->write.split ? 0 : -1;
		f->write.path   = file_format_name(f->write.fmt, &now);

		/* Open file */
		f->write.handle = file_reopen(&f->write);
		if (!f->write.handle)
			serror("Failed to open file for writing: '%s'", f->write.path);
	}

	return 0;
}

int file_close(struct node *n)
{
	struct file *f = n->_vd;
	
	free(f->read.path);
	free(f->write.path);

	if (f->read_timer)
		close(f->read_timer);
	if (f->read.handle)
		fclose(f->read.handle);
	if (f->write.handle)
		fclose(f->write.handle);

	return 0;
}

int file_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct file *f = n->_vd;
	struct sample *s = smps[0];
	int values, flags;

	assert(f->read.handle);
	assert(cnt == 1);

retry:	values = sample_fscan(f->read.handle, s, &flags); /* Get message and timestamp */
	if (values < 0) {
		if (feof(f->read.handle)) {
			if (f->read.split) {
				f->read.chunk++;
				f->read.handle = file_reopen(&f->read);
				if (!f->read.handle)
					return 0;
				
				info("Open new input chunk of node %s: %d", node_name(n), f->read.chunk);
			}
			else {
				info("Rewind input file of node %s", node_name(n));
				rewind(f->read.handle);
				goto retry;
			}
		}
		else
			warn("Failed to read messages from node %s: reason=%d", node_name(n), values);

		return 0;
	}

	if (!f->read_rate || ftell(f->read.handle) == 0) {
		s->ts.origin = time_add(&s->ts.origin, &f->read_offset);
		if (timerfd_wait_until(f->read_timer, &s->ts.origin) == 0)
			serror("Failed to wait for timer");
	}
	else { /* Wait with fixed rate delay */
		if (timerfd_wait(f->read_timer) == 0)
			serror("Failed to wait for timer");
	
		/* Update timestamp */
		s->ts.origin = time_now();
	}

	return 1;
}

int file_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct file *f = n->_vd;
	struct sample *s = smps[0];

	assert(f->write.handle);
	assert(cnt == 1);

	/* Split file if requested */
	if (f->write.split > 0 && ftell(f->write.handle) > f->write.split) {
		f->write.chunk++;
		f->write.handle = file_reopen(&f->write);
				
		info("Splitted output node %s: chunk=%u", node_name(n), f->write.chunk);
	}
		
	sample_fprint(f->write.handle, s, SAMPLE_ALL & ~SAMPLE_OFFSET);
	fflush(f->write.handle);

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
		.open		= file_open,
		.close		= file_close,
		.read		= file_read,
		.write		= file_write
	}
};

REGISTER_PLUGIN(&p)