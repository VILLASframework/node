/** Node type: File
 *
 * This file implements the file type for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 */

#include <unistd.h>
#include <sys/timerfd.h>

#include "file.h"
#include "utils.h"

int file_print(struct node *n, char *buf, int len)
{
	struct file *f = n->file;
	
	return snprintf(buf, len, "in=%s, out=%s, mode=%s, rate=%f",
		f->path_in, f->path_out, f->mode, f->rate);
}

int file_parse(config_setting_t *cfg, struct node *n)
{
	struct file *f = alloc(sizeof(struct file));
	
	config_setting_lookup_string(cfg, "in",  &f->path_in);
	config_setting_lookup_string(cfg, "out", &f->path_out);
	
	if (!config_setting_lookup_string(cfg, "mode", &f->mode))
		f->mode = "w+";
	
	if (!config_setting_lookup_float(cfg, "rate", &f->rate))
		f->rate = 1;
	
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
		
		struct itimerspec its = {
			.it_interval = timespec_rate(f->rate),
			.it_value = { 1, 0 }
		};
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
	
	return 0;
}

int file_read(struct node *n, struct msg *m)
{
	struct file *f = n->file;
	uint64_t runs;
	
	if (!f->in)
		error("Can't read from file node!");
	
	read(f->tfd, &runs, sizeof(runs)); /* blocking for 1/f->rate seconds */
	
	return msg_fscan(f->in, m);
}

int file_write(struct node *n, struct msg *m)
{
	struct file *f = n->file;
	
	if (!f->out)
		error("Can't write to file node!");
	
	return msg_fprint(f->out, m);
}