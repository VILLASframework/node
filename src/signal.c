/** Generate random packages on stdout.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *
 * @addtogroup tools Test and debug tools
 * @{
 **********************************************************************************/

#include <unistd.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "utils.h"
#include "msg.h"
#include "timing.h"

#define CLOCKID	CLOCK_REALTIME

enum SIGNAL_TYPE {
	TYPE_RANDOM,
	TYPE_SINE,
	TYPE_SQUARE,
	TYPE_TRIANGLE,
	TYPE_RAMP,
	TYPE_MIXED
};

void usage(char *name)
{
	printf("Usage: %s SIGNAL [OPTIONS]\n", name);
	printf("  SIGNAL   is on of: 'mixed', 'random', 'sine', 'triangle', 'square', 'ramp'\n");
	printf("  -v NUM   specifies how many values a message should contain\n");
	printf("  -r HZ    how many messages per second\n");
	printf("  -f HZ    the frequency of the signal\n");
	printf("  -a FLT   the amplitude\n");
	printf("  -d FLT   the standard deviation for 'random' signals\n");
	printf("  -l NUM   only send LIMIT messages and stop\n\n");

	print_copyright();
}

int main(int argc, char *argv[])
{
	/* Some default values */
	double rate = 10;
	double freq = 1;
	double ampl = 1;
	double stddev = 0.02;
	int type = TYPE_MIXED;
	int values = 1;
	int limit = -1;	

	int counter = 0;

	if (argc < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
		
	/* Parse signal type */
	if      (!strcmp(argv[1], "random"))
		type = TYPE_RANDOM;
	else if (!strcmp(argv[1], "sine"))
		type = TYPE_SINE;
	else if (!strcmp(argv[1], "square"))
		type = TYPE_SQUARE;
	else if (!strcmp(argv[1], "triangle"))
		type = TYPE_TRIANGLE;
	else if (!strcmp(argv[1], "ramp"))
		type = TYPE_RAMP;
	else if (!strcmp(argv[1], "mixed"))
		type = TYPE_MIXED;
	
	/* Parse optional command line arguments */
	char c, *endptr;
	while ((c = getopt(argc-1, argv+1, "hv:r:f:l:a:d:")) != -1) {
		switch (c) {
			case 'l':
				limit = strtoul(optarg, &endptr, 10);
				goto check;
			case 'v':
				values = strtoul(optarg, &endptr, 10);
				goto check;
			case 'r':
				rate   = strtof(optarg, &endptr);
				goto check;
			case 'f':
				freq   = strtof(optarg, &endptr);
				goto check;
			case 'a':
				ampl   = strtof(optarg, &endptr);
				goto check;
			case 'd':
				stddev = strtof(optarg, &endptr);
				goto check;
			case 'h':
			case '?':
				usage(argv[0]);
		}
		
		continue;
		
check:		if (optarg == endptr)
			error("Failed to parse parse option argument '-%c %s'", c, optarg);
	}
	
	/* Allocate memory for message buffer */
	struct msg *m = msg_create(values);

	/* Print header */
	printf("# S2SS signal params: type=%s, values=%u, rate=%f, limit=%u, amplitude=%f, freq=%f\n",
		argv[1], values, rate, limit, ampl, freq);
	printf("# %-20s\t\t%s\n", "sec.nsec(seq)", "data[]");

	/* Setup timer */
	int tfd = timerfd_create_rate(rate);
	if (tfd < 0)
		serror("Failed to create timer");

	struct timespec start = time_now();

	while (limit < 0 || counter < limit) {
		struct timespec now = time_now();
		double running = time_delta(&start, &now);

		m->ts.sec    = now.tv_sec;
		m->ts.nsec   = now.tv_nsec;
		m->sequence  = counter;

		for (int i = 0; i < m->values; i++) {
			int rtype = (type != TYPE_MIXED) ? type : i % 4;			
			switch (rtype) {
				case TYPE_RANDOM:   m->data[i].f += box_muller(0, stddev); 					break;
				case TYPE_SINE:	    m->data[i].f = ampl *        sin(running * freq * 2 * M_PI);		break;
				case TYPE_TRIANGLE: m->data[i].f = ampl * (fabs(fmod(running * freq, 1) - .5) - 0.25) * 4;	break;
				case TYPE_SQUARE:   m->data[i].f = ampl * (    (fmod(running * freq, 1) < .5) ? -1 : 1);	break;
				case TYPE_RAMP:     m->data[i].f = fmod(counter, rate / freq); /** @todo send as integer? */	break;
			}
		}
			
		msg_fprint(stdout, m, MSG_PRINT_ALL & ~MSG_PRINT_OFFSET, 0);
		fflush(stdout);
		
		/* Block until 1/p->rate seconds elapsed */
		counter += timerfd_wait(tfd);
	}

	close(tfd);
	free(m);

	return 0;
}

/** @} */
