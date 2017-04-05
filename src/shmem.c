/* Test "client" for the shared memory interface.
 * Busy waits on the incoming queue, prints received samples and writes them
 * back to the other queue. */

#include "config.h"
#include "cfg.h"
#include "log.h"
#include "nodes/shmem.h"
#include "pool.h"
#include "queue.h"
#include "sample.h"

#include <string.h>

void usage()
{
	printf("Usage: villas-shmem CONFIG NODE\n");
	printf("  CONFIG  path to a configuration file\n");
	printf("  NODE    the name of the node which samples should be read from\n");
}

int main(int argc, char* argv[])
{
	if (argc != 3) {
		usage();
		return 1;
	}
	struct list nodes;
	struct settings settings;
	config_t config;

	list_init(&nodes);
	cfg_parse(argv[1], &config, &settings, &nodes, NULL);

	struct node *node = list_lookup(&nodes, argv[2]);
	if (!node)
		error("Node '%s' does not exist!", argv[2]);

	struct shmem* shm = node->_vd;
	size_t len = shmem_total_size(shm->insize, shm->outsize, shm->sample_size);
	struct shmem_shared *shmem = shmem_int_open(shm->name, len);
	if (!shmem)
		serror("Failed to open shmem interface");

	struct sample *insmps[node->vectorize], *outsmps[node->vectorize];
	while (1) {
		if (shm->cond_out) {
			pthread_mutex_lock(&shmem->out.mt);
			pthread_cond_wait(&shmem->out.ready, &shmem->out.mt);
			pthread_mutex_unlock(&shmem->out.mt);
		}
		int r = queue_pull_many(&shmem->out.queue, (void **) insmps, node->vectorize);
		int avail = sample_alloc(&shmem->pool, outsmps, r);
		if (avail < r)
			warn("pool underrun (%d/%d)\n", avail, r);
		for (int i = 0; i < r; i++)
			sample_fprint(stdout, insmps[i], SAMPLE_ALL);
		for (int i = 0; i < avail; i++) {
			outsmps[i]->sequence = insmps[i]->sequence;
			outsmps[i]->ts = insmps[i]->ts;
			int len = MIN(insmps[i]->length, outsmps[i]->capacity);
			memcpy(outsmps[i]->data, insmps[i]->data, len*sizeof(insmps[0]->data[0]));
			outsmps[i]->length = len;
		}
		for (int i = 0; i < r; i++)
			sample_put(insmps[i]);
		int w = queue_push_many(&shmem->in.queue, (void **) outsmps, avail);
		if (w < avail)
			warn("short write (%d/%d)\n", w, r);
		if (shm->cond_in && w) {
			pthread_mutex_lock(&shmem->in.mt);
			pthread_cond_broadcast(&shmem->in.ready);
			pthread_mutex_unlock(&shmem->in.mt);
		}
	}
}
