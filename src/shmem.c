/* Test "client" for the shared memory interface.
 * Busy waits on the incoming queue, prints received samples and writes them
 * back to the other queue. */

#include "config.h"
#include "log.h"
#include "node.h"
#include "nodes/shmem.h"
#include "pool.h"
#include "queue.h"
#include "sample.h"
#include "sample_io.h"
#include "shmem.h"
#include "super_node.h"
#include "utils.h"

#include <string.h>

static struct super_node sn = { .state = STATE_DESTROYED };

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

	super_node_init(&sn);
	super_node_parse_uri(&sn, argv[1]);

	struct node *node = list_lookup(&sn.nodes, argv[2]);
	if (!node)
		error("Node '%s' does not exist!", argv[2]);

	struct shmem* shm = node->_vd;
	struct shmem_shared *shared = shmem_shared_open(shm->name);
	if (!shared)
		serror("Failed to open shmem interface");

	struct sample *insmps[node->vectorize], *outsmps[node->vectorize];
	while (1) {
		int r, w;
		r = shmem_shared_read(shared, insmps, node->vectorize);
		int avail = sample_alloc(&shared->pool, outsmps, r);
		if (avail < r)
			warn("pool underrun (%d/%d)\n", avail, r);
		for (int i = 0; i < r; i++)
			sample_io_villas_fprint(stdout, insmps[i], SAMPLE_IO_ALL);
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
