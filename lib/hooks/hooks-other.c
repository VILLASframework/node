/** Other hook funktions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "hooks.h"
#include "timing.h"
#include "utils.h"
#include "sample.h"

REGISTER_HOOK("print", "Print the message to stdout", 99, 0, hook_print, HOOK_READ)
int hook_print(struct path *p, struct hook *h, int when, struct sample *smps[], size_t cnt)
{
	for (int i = 0; i < cnt; i++)
		sample_fprint(stdout, smps[i], SAMPLE_ALL);

	return cnt;
}

REGISTER_HOOK("ts", "Update timestamp of message with current time", 99, 0, hook_ts, HOOK_READ)
int hook_ts(struct path *p, struct hook *h, int when, struct sample *smps[], size_t cnt)
{
	for (int i = 0; i < cnt; i++)
		smps[i]->ts.origin = smps[i]->ts.received;

	return cnt;
}

REGISTER_HOOK("convert", "Convert message from / to floating-point / integer", 99, 0, hook_convert, HOOK_STORAGE | HOOK_PARSE | HOOK_READ)
int hook_convert(struct path *p, struct hook *h, int when, struct sample *smps[], size_t cnt)
{
	struct {
		enum {
			TO_FIXED,
			TO_FLOAT
		} mode;
	} *private = hook_storage(h, when, sizeof(*private));

	switch (when) {
		case HOOK_PARSE:
			if (!h->parameter)
				error("Missing parameter for hook: '%s'", h->name);

			if      (!strcmp(h->parameter, "fixed"))
				private->mode = TO_FIXED;
			else if (!strcmp(h->parameter, "float"))
				private->mode = TO_FLOAT;
			else
				error("Invalid parameter '%s' for hook 'convert'", h->parameter);
			break;
		
		case HOOK_READ:
			for (int i = 0; i < cnt; i++) {
				for (int j = 0; j < smps[0]->length; j++) {
					switch (private->mode) {
						case TO_FIXED: smps[i]->data[j].i = smps[i]->data[j].f * 1e3; break;
						case TO_FLOAT: smps[i]->data[j].f = smps[i]->data[j].i; break;
					}
				}
			}
			break;
	}

	return 0;
}

REGISTER_HOOK("decimate", "Downsamping by integer factor", 99, 0, hook_decimate, HOOK_STORAGE | HOOK_PARSE | HOOK_READ)
int hook_decimate(struct path *p, struct hook *h, int when, struct sample *smps[], size_t cnt)
{
	struct {
		unsigned ratio;
		unsigned counter;
	} *private = hook_storage(h, when, sizeof(*private));

	int ok;

	switch (when) {
		case HOOK_PARSE:
			if (!h->parameter)
				error("Missing parameter for hook: '%s'", h->name);
	
			private->ratio = strtol(h->parameter, NULL, 10);
			if (!private->ratio)
				error("Invalid parameter '%s' for hook 'decimate'", h->parameter);
		
			private->counter = 0;
			break;
		
		case HOOK_READ:
			ok = 0;
			for (int i = 0; i < cnt; i++) {
				if (private->counter++ % private->ratio == 0) {
					struct sample *tmp;
					
					tmp = smps[ok];
					smps[ok++] = smps[i];
					smps[i] = tmp;
				}
			}
			return ok;
	}

	return cnt;
}

REGISTER_HOOK("skip_first", "Skip the first samples", 99, 0, hook_skip_first, HOOK_STORAGE |  HOOK_PARSE | HOOK_READ | HOOK_PATH)
int hook_skip_first(struct path *p, struct hook *h, int when, struct sample *smps[], size_t cnt)
{
	struct {
		struct timespec skip;	/**< Time to wait until first message is not skipped */
		struct timespec until;	/**< Absolute point in time from where we accept samples. */
	} *private = hook_storage(h, when, sizeof(*private));

	char *endptr;
	double wait;
	int i, ok;

	switch (when) {
		case HOOK_PARSE:
			if (!h->parameter)
				error("Missing parameter for hook: '%s'", h->name);

			wait = strtof(h->parameter, &endptr);
			if (h->parameter == endptr)
				error("Invalid parameter '%s' for hook 'skip_first'", h->parameter);
	
			private->skip = time_from_double(wait);
			break;
	
		case HOOK_PATH_RESTART:
		case HOOK_PATH_STOP:
			private->until = time_add(&smps[0]->ts.received, &private->skip);
			break;
	
		case HOOK_READ:
			for (i = 0, ok = 0; i < cnt; i++) {
				if (time_delta(&private->until, &smps[i]->ts.received) > 0) {
					struct sample *tmp;

					tmp = smps[i];
					smps[i] = smps[ok];
					smps[ok++] = tmp;
				
				}

				/* To discard the first X samples in 'smps[]' we must
				 * shift them to the end of the 'smps[]' array.
				 * In case the hook returns a number 'ok' which is smaller than 'cnt',
				 * only the first 'ok' samples in 'smps[]' are accepted and further processed.
				 */
			}

			return ok;
	}

	return 0;
}