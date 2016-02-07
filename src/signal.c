/** Generate random packages on stdout.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
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

int main(int argc, char *argv[])
{
	if (argc < 4 || argc > 5) {
		printf("Usage: %s SIGNAL VALUES RATE [LIMIT]\n", argv[0]);
		printf("  SIGNAL is on of: mixed random sine triangle square ramp\n");
		printf("  VALUES is the number of values a message contains\n");
		printf("  RATE   how many messages per second\n");
		printf("  LIMIT  only send LIMIT messages\n\n");

		print_copyright();

		exit(EXIT_FAILURE);
	}

	double rate = atof(argv[3]);
	int type = TYPE_MIXED;
	int values = atoi(argv[2]);
	int limit = argc >= 5 ? atoi(argv[4]) : -1;
	int counter = 0;
	
	struct msg *m = msg_create(values);
	
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

	/* Print header */
	fprintf(stderr, "# %-20s\t\t%s\n", "sec.nsec(seq)", "data[]");


	/* Setup timer */
	int tfd = timerfd_create_rate(rate);
	if (tfd < 0)
		serror("Failed to create timer");

	struct timespec start = time_now();

	while (counter < limit || argc < 5) {
		struct timespec now = time_now();
		double running = time_delta(&start, &now);

		m->ts.sec    = now.tv_sec;
		m->ts.nsec   = now.tv_nsec;
		m->sequence  = counter;

		for (int i = 0; i < m->values; i++) {
			int rtype   = (type != TYPE_MIXED) ? type : i % 4;
			double ampl = i+1;
			double freq = i+1;

			switch (rtype) {
				case TYPE_RANDOM:   m->data[i].f += box_muller(0, 0.02); 					break;
				case TYPE_SINE:	    m->data[i].f = ampl *        sin(running * freq * M_PI);			break;
				case TYPE_TRIANGLE: m->data[i].f = ampl * (fabs(fmod(running * freq, 1) - .5) - 0.25) * 4;	break;
				case TYPE_SQUARE:   m->data[i].f = ampl * (    (fmod(running * freq, 1) < .5) ? -1 : 1);	break;
				case TYPE_RAMP:     m->data[i].f = counter; /** @todo send as integer? */			break;
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
