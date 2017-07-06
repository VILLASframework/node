/** Generate random packages on stdout.
 *
 * @file
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
 *
 * @addtogroup tools Test and debug tools
 * @{
 **********************************************************************************/

#include <unistd.h>
#include <math.h>
#include <string.h>

#include <villas/utils.h>
#include <villas/sample.h>
#include <villas/sample_io.h>
#include <villas/timing.h>
#include <villas/node.h>
#include <villas/nodes/signal.h>

/* Some default values */
struct signal s = {
	.rate = 10,
	.frequency = 1,
	.amplitude = 1,
	.stddev = 0.02,
	.type = SIGNAL_TYPE_MIXED,
	.rt = 1,
	.values = 1,
	.limit = -1
};

struct node n = {
	._vd = &s
};

struct log l;

void usage()
{
	printf("Usage: villas-signal [OPTIONS] SIGNAL\n");
	printf("  SIGNAL   is on of the following signal types:\n");
	printf("    mixed\n");
	printf("    random\n");
	printf("    sine\n");
	printf("    triangle\n");
	printf("    square\n");
	printf("    ramp\n");
	printf("\n");
	printf("  OPTIONS is one or more of the following options:\n");
	printf("    -d LVL   set debug level\n");
	printf("    -v NUM   specifies how many values a message should contain\n");
	printf("    -r HZ    how many messages per second\n");
	printf("    -n       non real-time mode. do not throttle output.\n");
	printf("    -f HZ    the frequency of the signal\n");
	printf("    -a FLT   the amplitude\n");
	printf("    -D FLT   the standard deviation for 'random' signals\n");
	printf("    -l NUM   only send LIMIT messages and stop\n\n");

	print_copyright();
}

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	signal_close(&n);

	info(GRN("Goodbye!"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	char *type;
	int ret, level = V;

	/* Parse optional command line arguments */
	char c, *endptr;
	while ((c = getopt(argc, argv, "hv:r:f:l:a:D:d:n")) != -1) {
		switch (c) {
			case 'd':
				level = strtoul(optarg, &endptr, 10);
				goto check;
			case 'n':
				s.rt = 0;
				break;
			case 'l':
				s.limit = strtoul(optarg, &endptr, 10);
				goto check;
			case 'v':
				s.values = strtoul(optarg, &endptr, 10);
				goto check;
			case 'r':
				s.rate = strtof(optarg, &endptr);
				goto check;
			case 'f':
				s.frequency = strtof(optarg, &endptr);
				goto check;
			case 'a':
				s.amplitude = strtof(optarg, &endptr);
				goto check;
			case 'D':
				s.stddev = strtof(optarg, &endptr);
				goto check;
			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;

check:		if (optarg == endptr)
			error("Failed to parse parse option argument '-%c %s'", c, optarg);
	}

	if (argc != optind + 1) {
		usage();
		exit(EXIT_FAILURE);
	}
	
	type = argv[optind];

	s.type = signal_lookup_type(type);
	if (s.type == -1)
		error("Invalid signal type: %s", type);

	log_init(&l, level, LOG_ALL);
	signals_init(quit);
	
	info("Starting signal generation: %s", signal_print(&n));

	/* Allocate memory for message buffer */
	struct sample *t = alloc(SAMPLE_LEN(s.values));

	t->capacity = s.values;

	/* Print header */
	printf("# VILLASnode signal params: type=%s, values=%u, rate=%f, limit=%d, amplitude=%f, freq=%f\n",
		argv[optind], s.values, s.rate, s.limit, s.amplitude, s.frequency);
	printf("# %-20s\t\t%s\n", "sec.nsec(seq)", "data[]");

	ret = signal_open(&n);
	if (ret)
		serror("Failed to start node");

	for (;;) {
		signal_read(&n, &t, 1);

		sample_io_villas_fprint(stdout, t, SAMPLE_IO_ALL & ~SAMPLE_IO_OFFSET);
		fflush(stdout);
	}

	return 0;
}

/** @} */
