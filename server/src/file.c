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

	return strcatf(&buf, "in=%s, out=%s, mode=%s, rate=%.1f, epoch_mode=%u, epoch=%.0f",
		f->path_in, f->path_out, f->file_mode, f->rate, f->epoch_mode, time_to_double(&f->epoch));
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
		epoch_mode = "now";

	if (!strcmp(epoch_mode, "now"))
		f->epoch_mode = EPOCH_NOW;
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
		f->in  = fopen(f->path_in,  "r");
		if (!f->in)
			serror("Failed to open file for reading: '%s'", f->path_in);

		f->tfd = timerfd_create(CLOCK_REALTIME, 0);
		if (f->tfd < 0)
			serror("Failed to create timer");

		/* Arm the timer */
		if (f->rate) {
			/* Send with fixed rate */
			struct itimerspec its = {
				.it_interval = time_from_double(1 / f->rate),
				.it_value = { 1, 0 },
			};

			int ret = timerfd_settime(f->tfd, 0, &its, NULL);
			if (ret)
				serror("Failed to start timer");
		}
		else {
			/* Get current time */
			struct timespec now, tmp;
			clock_gettime(CLOCK_REALTIME, &now);

			/* Get timestamp of first sample */
			time_fscan(f->in, &f->start); rewind(f->in);

			/* Set offset depending on epoch_mode */
			switch (f->epoch_mode) {
				case EPOCH_NOW: /* read first value at f->now + f->epoch */
					tmp = time_diff(&f->start, &now);
					f->offset = time_add(&tmp, &f->epoch);
					break;
				case EPOCH_RELATIVE: /* read first value at f->start + f->epoch */
					f->offset = f->epoch;
					break;
				case EPOCH_ABSOLUTE: /* read first value at f->epoch */
					f->offset = time_diff(&f->start, &f->epoch);
					break;
			}

			tmp = time_add(&f->start, &f->offset);
			tmp = time_diff(&now, &tmp);

			debug(5, "Opened file '%s' as input for node '%s': start=%.2f, offset=%.2f, eta=%.2f",
				f->path_in, n->name,
				time_to_double(&f->start),
				time_to_double(&f->offset),
				time_to_double(&tmp)
			);
		}
	}

	if (f->path_out) {
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
	int i = 0;
	struct file *f = n->file;

	if (f->in) {
		for (i = 0; i < cnt; i++) {
			struct msg *cur = &pool[(first+i) % poolsize];
			
			if (f->rate) {
				/* Wait until epoch for the first time only */
				if (ftell(f->in) == 0) {
					struct timespec until = time_add(&f->start, &f->offset);
					if (timerfd_wait_until(f->tfd, &until))
						serror("Failed to wait for timer");
				}
				/* Wait with fixed rate delay */
				else {
					if (timerfd_wait(f->tfd) < 0)
						serror("Failed to wait for timer");
				}

				msg_fscan(f->in, cur, NULL, NULL);
			}
			else {
				struct timespec until;
			
				/* Get message and timestamp */
				msg_fscan(f->in, cur, NULL, NULL);

				/* Wait for next message / sampe */
				until = time_add(&MSG_TS(cur), &f->offset);
				if (timerfd_wait_until(f->tfd, &until) < 0)
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