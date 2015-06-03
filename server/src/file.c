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

int file_print(struct node *n, char *buf, int len)
{
	struct file *f = n->file;
	
	return snprintf(buf, len, "in=%s, out=%s, mode=%s, rate=%.1f",
		f->path_in, f->path_out, f->mode, f->rate);
}

int file_parse(config_setting_t *cfg, struct node *n)
{
	struct file *f = alloc(sizeof(struct file));
	
	const char *out;
	if (config_setting_lookup_string(cfg, "out", &out)) {
		time_t t = time(NULL);
		struct tm *tm = localtime(&t);
		
		f->path_out = alloc(FILE_MAX_PATHLEN);
		if (strftime(f->path_out, FILE_MAX_PATHLEN, out, tm) == 0)
			cerror(cfg, "Invalid path for output");
			
	}
	config_setting_lookup_string(cfg, "in",  &f->path_in);
	
	if (!config_setting_lookup_string(cfg, "mode", &f->mode))
		f->mode = "w+";
	
	if (!config_setting_lookup_float(cfg, "rate", &f->rate))
		f->rate = 0;
	
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
		
		f->tfd = timerfd_create(CLOCK_MONOTONIC, 0);
		if (f->tfd < 0)
			serror("Failed to create timer");
		
		/* Arm the timer */
		struct itimerspec its;
		if (f->rate) {
			/* Send with fixed rate */
			its.it_interval = time_from_double(1 / f->rate);
			its.it_value = (struct timespec) { 1, 0 };
		}
		else {
			/* Read timestamp from first line to get an epoch offset */
			time_fscan(f->in, &f->offset);
			rewind(f->in);
		}
		
		int ret = timerfd_settime(f->tfd, 0, &its, NULL);
		if (ret)
			serror("Failed to start timer");
	}
		
	if (f->path_out) {
		f->out = fopen(f->path_out, f->mode);
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
	
	free(f->path_out);
	
	return 0;
}

int file_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	int i = 0;
	struct file *f = n->file;
	
	if (f->in) {
		struct timespec ts;

		for (i = 0; i < cnt; i++)
			msg_fscan(f->in, &pool[(first+i) % poolsize]);
		
		if (f->rate)
			timerfd_wait(f->tfd);
		else
			timerfd_wait_until(f->tfd, &ts);
		
	}
	else
		warn("Can not read from node '%s'", n->name);
	
	return i;
}

int file_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	int i = 0;
	struct file *f = n->file;
	
	if (f->out) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);

		for (i = 0; i < cnt; i++)
			msg_fprint(f->out, &pool[(first+i) % poolsize]);
	}
	else
		warn("Can not write to node '%s", n->name);
	
	return i;
}