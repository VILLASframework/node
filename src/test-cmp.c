/** Compare two data files.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <getopt.h>

#include <jansson.h>

#include <villas/sample.h>
#include <villas/sample_io.h>
#include <villas/utils.h>
#include <villas/hist.h>
#include <villas/timing.h>
#include <villas/pool.h>

#include "config.h"

void usage()
{
	printf("Usage: villas-test-cmp FILE1 FILE2 [OPTIONS]\n");
	printf("  FILE1    first file to compare\n");
	printf("  FILE2    second file to compare against\n");
	printf("  OPTIONS  the following optional options:\n");
	printf("   -h      print this usage information\n");
	printf("   -d LVL  adjust the debug level\n");
	printf("   -j      return the results as a JSON object\n");
	printf("   -m      return the results as a MATLAB struct\n");
	printf("   -e EPS  set epsilon for floating point comparisons to EPS\n");
	printf("   -l LOW  smallest value for histogram\n");
	printf("   -H HIGH largest value for histogram\n");
	printf("   -r RES  bucket resolution for histogram\n");
	printf("\n");

	print_copyright();	
}

int main(int argc, char *argv[])
{
	int ret = 0;
	double epsilon = 1e-9;
	
	/* Histogram */
	double low = 0;		/**< Lowest value in histogram. */
	double high = 1e-1;	/**< Highest value in histogram. */
	double res = 1e-3;	/**< Histogram resolution. */
	
	enum {
		OUTPUT_JSON,
		OUTPUT_MATLAB,
		OUTPUT_HUMAN
	} output = OUTPUT_HUMAN;
	
	struct log log;
	struct pool pool = { .state = STATE_DESTROYED };
	struct hist hist;
	struct sample *samples[2];
	
	struct {
		int flags;
		char *path;
		FILE *handle;
		struct sample *sample;
	} f1, f2;

	/* Parse Arguments */
	char c, *endptr;
	while ((c = getopt (argc, argv, "hjmd:e:l:H:r:")) != -1) {
		switch (c) {
			case 'd':
				log.level = strtoul(optarg, &endptr, 10);
				goto check;
			case 'e':
				epsilon = strtod(optarg, &endptr);
				goto check;
			case 'j':
				output = OUTPUT_JSON;
				break;
			case 'm':
				output = OUTPUT_MATLAB;
				break;
			case 'l':
				low = strtod(optarg, &endptr);
				goto check;
			case 'H':
				high = strtod(optarg, &endptr);
				goto check;
			case 'r':
				res = strtod(optarg, &endptr);
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
	
	if (argc < optind + 2) {
		usage();
		exit(EXIT_FAILURE);
	}
	
	f1.path = argv[optind];
	f2.path = argv[optind + 1];

	log_init(&log, V, LOG_ALL);
	log_start(&log);

	hist_init(&hist, low, high, res);
	pool_init(&pool, 1024, SAMPLE_LEN(DEFAULT_VALUES), &memtype_heap);
	sample_alloc(&pool, samples, 2);
	
	f1.sample = samples[0];
	f2.sample = samples[1];
	
	f1.handle = fopen(f1.path, "r");
	if (!f1.handle)
		serror("Failed to open file: %s", f1.path);

	f2.handle = fopen(f2.path, "r");
	if (!f2.handle)
		serror("Failed to open file: %s", f2.path);
	
	while (!feof(f1.handle) && !feof(f2.handle)) {
		ret = sample_io_villas_fscan(f1.handle, f1.sample, &f1.flags);
		if (ret < 0) {
			if (feof(f1.handle))
				ret = 0;
			goto out;
		}
		
		ret = sample_io_villas_fscan(f2.handle, f2.sample, &f2.flags);
		if (ret < 0) {
			if (feof(f2.handle))
				ret = 0;
			goto out;
		}

		/* Compare sequence no */
		if ((f1.flags & SAMPLE_IO_SEQUENCE) && (f2.flags & SAMPLE_IO_SEQUENCE)) {
			if (f1.sample->sequence != f2.sample->sequence) {
				printf("sequence no: %d != %d\n", f1.sample->sequence, f2.sample->sequence);
				ret = -1;
				goto out;
			}
		}
		
		/* Compare timestamp */
		if (time_delta(&f1.sample->ts.origin, &f2.sample->ts.origin) > epsilon) {
			printf("ts.origin: %f != %f\n", time_to_double(&f1.sample->ts.origin), time_to_double(&f2.sample->ts.origin));
			ret = -2;
			goto out;
		}
		
		/* Collect historgram info of offset */
		hist_put(&hist, time_delta(&f1.sample->ts.origin, &f2.sample->ts.received));

		/* Compare data */
		if (f1.sample->length != f2.sample->length) {
			printf("length: %d != %d\n", f1.sample->length, f2.sample->length);
			ret = -3;
			goto out;
		}
		
		for (int i = 0; i < f1.sample->length; i++) {
			if (fabs(f1.sample->data[i].f - f2.sample->data[i].f) > epsilon) {
				printf("data[%d]: %f != %f\n", i, f1.sample->data[i].f, f2.sample->data[i].f);
				ret = -4;
				goto out;
			}
		}
	}
	
out:	sample_free(samples, 2);

	fclose(f1.handle);
	fclose(f2.handle);
	
	switch (output) {
		case OUTPUT_MATLAB:
			hist_dump_matlab(&hist, stdout);
			break;
		case OUTPUT_JSON:
			hist_dump_json(&hist, stdout);
			break;
		case OUTPUT_HUMAN:
			hist_print(&hist, 1);
			break;
	}

	hist_destroy(&hist);
	pool_destroy(&pool);

	return ret;
}