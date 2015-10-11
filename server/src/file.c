/** Node type: File
 *
 * This file implements the file type for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <unistd.h>
#include <string.h>

#include "msg.h"
#include "file.h"
#include "utils.h"
#include "timing.h"

int file_init(int argc, char *argv[], struct settings *set)
{ INDENT
	return 0; /* nothing todo here */
}

int file_deinit()
{ INDENT
	return 0; /* nothing todo here */
}

char * file_print(struct node *n)
{
	struct file *f = n->file;
	char *buf = NULL;
	
	if (f->path_in) {
		const char *epoch_mode_str = NULL;
		switch (f->epoch_mode) {
			case EPOCH_DIRECT:	epoch_mode_str = "direct"; break;
			case EPOCH_WAIT:	epoch_mode_str = "wait"; break;
			case EPOCH_RELATIVE:	epoch_mode_str = "relative"; break;
			case EPOCH_ABSOLUTE:	epoch_mode_str = "absolute"; break;
		}
		
		strcatf(&buf, "in=%s, epoch_mode=%s, epoch=%.2f, ",
			f->path_in,
			epoch_mode_str,
			time_to_double(&f->epoch)
		);
			
		if (f->rate)
			strcatf(&buf, "rate=%.1f, ", f->rate);
	}
	
	if (f->path_out) {
		strcatf(&buf, "out=%s, mode=%s, ",
			f->path_out,
			f->file_mode
		);
	}
	
	if (f->first.tv_sec || f->first.tv_nsec)
		strcatf(&buf, "first=%.2f, ", time_to_double(&f->first));
	
	if (f->offset.tv_sec || f->offset.tv_nsec)
		strcatf(&buf, "offset=%.2f, ", time_to_double(&f->offset));
	
	if ((f->first.tv_sec || f->first.tv_nsec) &&
	    (f->offset.tv_sec || f->offset.tv_nsec)) {
    		struct timespec eta, now = time_now();

    		eta = time_add(&f->first, &f->offset);
    		eta = time_diff(&now, &eta);

		if (eta.tv_sec || eta.tv_nsec)
		strcatf(&buf, "eta=%.2f sec, ", time_to_double(&eta));
	}
	
	if (strlen(buf) > 2)
		buf[strlen(buf) - 2] = 0;

	return buf;
}

int file_parse(config_setting_t *cfg, struct node *n)
{
	struct file *f = alloc(sizeof(struct file));

	const char *out, *in;
	const char *epoch_mode;
	double epoch_flt;

	if (config_setting_lookup_string(cfg, "out", &out)) {
		time_t t = time(NULL);
		struct tm *tm = localtime(&t);

		f->path_out = alloc(FILE_MAX_PATHLEN);
		if (strftime(f->path_out, FILE_MAX_PATHLEN, out, tm) == 0)
			cerror(cfg, "Invalid path for output");

	}
	if (config_setting_lookup_string(cfg, "in", &in))
		f->path_in = strdup(in);

	if (!config_setting_lookup_string(cfg, "file_mode", &f->file_mode))
		f->file_mode = "w+";

	if (!config_setting_lookup_float(cfg, "send_rate", &f->rate))
		f->rate = 0; /* Disable fixed rate sending. Using timestamps of file instead */

	if (config_setting_lookup_float(n->cfg, "epoch", &epoch_flt))
		f->epoch = time_from_double(epoch_flt);

	if (!config_setting_lookup_string(n->cfg, "epoch_mode", &epoch_mode))
		epoch_mode = "direct";

	if (!strcmp(epoch_mode, "direct"))
		f->epoch_mode = EPOCH_DIRECT;
	else if (!strcmp(epoch_mode, "wait"))
		f->epoch_mode = EPOCH_WAIT;
	else if (!strcmp(epoch_mode, "relative"))
		f->epoch_mode = EPOCH_RELATIVE;
	else if (!strcmp(epoch_mode, "absolute"))
		f->epoch_mode = EPOCH_ABSOLUTE;
	else
		cerror(n->cfg, "Invalid value '%s' for setting 'epoch_mode'", epoch_mode);

	n->file = f;

	return 0;
}

int file_open(struct node *n)
{
	struct file *f = n->file;

	if (f->path_in) {
		/* Open file */
		f->in = fopen(f->path_in,  "r");
		if (!f->in)
			serror("Failed to open file for reading: '%s'", f->path_in);

		/* Create timer */
		f->tfd = timerfd_create(CLOCK_REALTIME, 0);
		if (f->tfd < 0)
			serror("Failed to create timer");
		
		/* Get current time */
		struct timespec now = time_now();

		/* Get timestamp of first line */
		struct msg m;
		int ret = msg_fscan(f->in, &m, NULL, NULL); rewind(f->in);
		if (ret < 0)
			error("Failed to read first timestamp of node '%s'", n->name);
		
		f->first = MSG_TS(&m);

		/* Set offset depending on epoch_mode */
		switch (f->epoch_mode) {
			case EPOCH_DIRECT: /* read first value at now + epoch */
				f->offset = time_diff(&f->first, &now);
				f->offset = time_add(&f->offset, &f->epoch);
				break;

			case EPOCH_WAIT: /* read first value at now + first + epoch */
				f->offset = now;
				f->offset = time_add(&f->offset, &f->epoch);
				break;
		
			case EPOCH_RELATIVE: /* read first value at first + epoch */
				f->offset = f->epoch;
				break;
		
			case EPOCH_ABSOLUTE: /* read first value at f->epoch */
				f->offset = time_diff(&f->first, &f->epoch);
				break;
		}
		
		/* Arm the timer with a fixed rate */
		if (f->rate) {
			struct itimerspec its = {
				.it_interval = time_from_double(1 / f->rate),
				.it_value = { 1, 0 },
			};

			int ret = timerfd_settime(f->tfd, 0, &its, NULL);
			if (ret)
				serror("Failed to start timer");
		}
		
		char *buf = file_print(n);
		debug(4, "Opened file node '%s': %s", n->name, buf);
		free(buf);
	}

	if (f->path_out) {
		/* Open output file */
		f->out = fopen(f->path_out, f->file_mode);
		if (!f->out)
			serror("Failed to open file for writing: '%s'", f->path_out);
	}

	return 0;
}

int file_close(struct node *n)
{
	struct file *f = n->file;

	if (f->tfd)
		close(f->tfd);
	if (f->in)
		fclose(f->in);
	if (f->out)
		fclose(f->out);

	return 0;
}

int file_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	int values, flags, i = 0;
	struct file *f = n->file;

	if (f->in) {
		for (i = 0; i < cnt; i++) {
			struct msg *cur = &pool[(first+i) % poolsize];
			
			/* Get message and timestamp */
			values = msg_fscan(f->in, cur, &flags, NULL);
			if (values < 0) {
				if (!feof(f->in))
					warn("Failed to parse file of node '%s': reason=%d", n->name, values);

				return 0;
			}
			
			/* Fix missing sequence no */
			cur->sequence = f->sequence = (flags & MSG_PRINT_SEQUENCE) ? cur->sequence : f->sequence + 1;

			if (!f->rate || ftell(f->in) == 0) {
				struct timespec until = time_add(&MSG_TS(cur), &f->offset);

				if (timerfd_wait_until(f->tfd, &until) < 0)
					serror("Failed to wait for timer");
			}
			else { /* Wait with fixed rate delay */
				if (timerfd_wait(f->tfd) < 0)
					serror("Failed to wait for timer");
			}
		}
	}
	else
		error("Can not read from node '%s'", n->name);

	return i;
}

int file_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	int i = 0;
	struct file *f = n->file;

	if (f->out) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);

		for (i = 0; i < cnt; i++) {
			struct msg *m = &pool[(first+i) % poolsize];
			msg_fprint(f->out, m, MSG_PRINT_ALL & ~MSG_PRINT_OFFSET, 0);
		}
	}
	else
		error("Can not write to node '%s", n->name);

	return i;
}

REGISTER_NODE_TYPE(LOG_FILE, "file", file)