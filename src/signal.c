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
#include <villas/plugin.h>
#include <villas/nodes/signal.h>

/* Some default values */
struct node n;
struct log l;

struct sample *t;

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
	int ret;

	ret = node_stop(&n);
	if (ret)
		error("Failed to stop node");

	ret = node_destroy(&n);
	if (ret)
		error("Failed to destroy node");

	ret = log_stop(&l);
	if (ret)
		error("Failed to stop log");

	free(t);

	info(CLR_GRN("Goodbye!"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	int ret;
	struct plugin *p;
	struct signal *s;

	ret = log_init(&l, l.level, LOG_ALL);
	if (ret)
		error("Failed to initialize log");

	ret = log_start(&l);
	if (ret)
		error("Failed to start log");

	ret = signals_init(quit);
	if (ret)
		error("Failed to intialize signals");

	p = plugin_lookup(PLUGIN_TYPE_NODE, "signal");
	if (!p)
		error("Signal generation is not supported.");

	ret = node_init(&n, &p->node);
	if (ret)
		error("Failed to initialize node");

	ret = node_parse_cli(&n, argc, argv);
	if (ret)
		error("Failed to parse command line options");

	ret = node_check(&n);
	if (ret)
		error("Failed to verify node configuration");

	info("Starting signal generation: %s", node_name(&n));

	/* Allocate memory for message buffer */
	s = n._vd;

	t = alloc(SAMPLE_LEN(s->values));

	t->capacity = s->values;

	/* Print header */
	printf("# VILLASnode signal params: type=%s, values=%u, rate=%f, limit=%d, amplitude=%f, freq=%f\n",
		argv[optind], s->values, s->rate, s->limit, s->amplitude, s->frequency);
	printf("# %-20s\t\t%s\n", "sec.nsec(seq)", "data[]");

	ret = node_start(&n);
	if (ret)
		serror("Failed to start node");

	for (;;) {
		node_read(&n, &t, 1);

		sample_io_villas_fprint(stdout, t, SAMPLE_IO_ALL & ~SAMPLE_IO_OFFSET);
		fflush(stdout);
	}

	return 0;
}

/** @} */
