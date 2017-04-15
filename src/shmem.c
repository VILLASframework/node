/* Test "client" for the shared memory interface.
 * Busy waits on the incoming queue, prints received samples and writes them
 * back to the other queue. */

#include "config.h"
#include "log.h"
#include "node.h"
#include "nodes/shmem.h"
#include "pool.h"
#include "queue_signalled.h"
#include "sample.h"
#include "shmem.h"
#include "utils.h"

#include <string.h>

#define VECTORIZE 8

void *base;
struct shmem_shared *shared;

void usage()
{
	printf("Usage: villas-shmem SHM_NAME\n");
	printf("  SHMNAME name of the shared memory object\n");
}

void quit(int sig)
{
	shmem_shared_close(shared, base);
	exit(1);
}

int main(int argc, char* argv[])
{
	if (argc != 2) {
		usage();
		return 1;
	}

	shared = shmem_shared_open(argv[1], &base);
	if (!shared)
		serror("Failed to open shmem interface");

	signal(SIGINT, quit);
	signal(SIGTERM, quit);
	struct sample *insmps[VECTORIZE], *outsmps[VECTORIZE];
	while (1) {
		int r, w;
		r = shmem_shared_read(shared, insmps, VECTORIZE);
		if (r == -1) {
			printf("node stopped, exiting\n");
			break;
		}
		int avail = sample_alloc(&shared->pool, outsmps, r);
		if (avail < r)
			warn("pool underrun (%d/%d)\n", avail, r);
		for (int i = 0; i < r; i++) {
			printf("got sample: seq %d recv %ld.%ld\n", insmps[i]->sequence,
			       insmps[i]->ts.received.tv_sec, insmps[i]->ts.received.tv_nsec);
		}
		for (int i = 0; i < avail; i++) {
			outsmps[i]->sequence = insmps[i]->sequence;
			outsmps[i]->ts = insmps[i]->ts;
			int len = MIN(insmps[i]->length, outsmps[i]->capacity);
			memcpy(outsmps[i]->data, insmps[i]->data, len*sizeof(insmps[0]->data[0]));
			outsmps[i]->length = len;
		}
		for (int i = 0; i < r; i++)
			sample_put(insmps[i]);
		w = shmem_shared_write(shared, outsmps, avail);
		if (w < avail)
			warn("short write (%d/%d)\n", w, r);
	}
}
