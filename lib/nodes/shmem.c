#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kernel/kernel.h"
#include "log.h"
#include "nodes/shmem.h"

int shmem_parse(struct node *n, config_setting_t *cfg) {
	struct shmem *shm = n->_vd;

	if (!config_setting_lookup_string(cfg, "name", &shm->name))
		cerror(cfg, "Missing shm object name");

	if (!config_setting_lookup_int(cfg, "insize", &shm->insize))
		shm->insize = DEFAULT_SHMEM_QUEUESIZE;
	if (!config_setting_lookup_int(cfg, "outsize", &shm->outsize))
		shm->outsize = DEFAULT_SHMEM_QUEUESIZE;
	if (!config_setting_lookup_int(cfg, "sample_size", &shm->sample_size))
		cerror(cfg, "Missing sample size setting");
	if (!config_setting_lookup_bool(cfg, "cond_out", &shm->cond_out))
		shm->cond_out = false;
	if (!config_setting_lookup_bool(cfg, "cond_in", &shm->cond_in))
		shm->cond_in = false;

	return 0;
}

/* Helper for initializing condition variables / mutexes. */
void shmem_cond_init(struct shmem_queue *queue) {
	pthread_mutexattr_init(&queue->mtattr);
	pthread_mutexattr_setpshared(&queue->mtattr, PTHREAD_PROCESS_SHARED);
	pthread_condattr_init(&queue->readyattr);
	pthread_condattr_setpshared(&queue->readyattr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&queue->mt, &queue->mtattr);
	pthread_cond_init(&queue->ready, &queue->readyattr);
}

int shmem_open(struct node *n) {
	struct shmem *shm = n->_vd;

	int r = shm_open(shm->name, O_RDWR|O_CREAT, 0600);
	if (r < 0)
		serror("Opening shared memory object failed");

	shm->fd = r;
	shm->len = shmem_total_size(shm->insize, shm->outsize, shm->sample_size);
	if (ftruncate(shm->fd, shm->len) < 0)
		serror("Setting size of shared memory object failed");
	/* TODO: we could use huge pages here as well */
	shm->base = mmap(NULL, shm->len, PROT_READ|PROT_WRITE, MAP_SHARED, shm->fd, 0);

	if (shm->base == MAP_FAILED)
		serror("Mapping shared memory failed");

	shm->manager = memtype_managed_init(shm->base, shm->len);
	shm->shared = memory_alloc(shm->manager, sizeof(struct shmem_shared));
	if (!shm->shared)
		error("Shm shared struct allocation failed (not enough memory?");
	queue_init(&shm->shared->in.queue, shm->insize, shm->manager);
	queue_init(&shm->shared->out.queue, shm->outsize, shm->manager);
	pool_init(&shm->shared->pool, shm->insize+shm->outsize, SAMPLE_LEN(shm->sample_size), shm->manager);
	if (shm->cond_out)
		shmem_cond_init(&shm->shared->out);
	if (shm->cond_in)
		shmem_cond_init(&shm->shared->in);

	return 0;
}

int shmem_close(struct node *n) {
	struct shmem* shm = n->_vd;
	int r = munmap(shm->base, shm->len);
	if (r != 0)
		return r;
	return shm_unlink(shm->name);
}

int shmem_read(struct node *n, struct sample *smps[], unsigned cnt) {
	struct shmem *shm = n->_vd;
	if (shm->cond_in) {
		pthread_mutex_lock(&shm->shared->in.mt);
		pthread_cond_wait(&shm->shared->in.ready, &shm->shared->in.mt);
		pthread_mutex_unlock(&shm->shared->in.mt);
	}
	int r = queue_pull_many(&shm->shared->in.queue, (void**) smps, cnt);
	return r;
}

int shmem_write(struct node *n, struct sample *smps[], unsigned cnt) {
	struct shmem *shm = n->_vd;

	/* Samples need to be copied to the shared pool first */
	struct sample *shared_smps[cnt];
	int avail = sample_alloc(&shm->shared->pool, shared_smps, cnt);
	if (avail != cnt)
		warn("Pool underrun for shmem node %s", shm->name);
	for (int i = 0; i < avail; i++) {
		/* Since the node isn't in shared memory, the source can't be accessed */
		shared_smps[i]->source = NULL;
		shared_smps[i]->sequence = smps[i]->sequence;
		shared_smps[i]->ts = smps[i]->ts;
		int len = MIN(smps[i]->length, shared_smps[i]->capacity);
		if (len != smps[i]->length)
			warn("Losing data because of sample capacity mismatch in shmem node %s", shm->name);
		memcpy(shared_smps[i]->data, smps[i]->data, len*sizeof(smps[0]->data[0]));
		shared_smps[i]->length = len;
		sample_get(shared_smps[i]);
	}
	int pushed = queue_push_many(&shm->shared->out.queue, (void**) shared_smps, avail);
	if (pushed != avail)
		warn("Outqueue overrun for shmem node %s", shm->name);
	if (pushed && shm->cond_out) {
		pthread_mutex_lock(&shm->shared->out.mt);
		pthread_cond_broadcast(&shm->shared->out.ready);
		pthread_mutex_unlock(&shm->shared->out.mt);
	}
	return pushed;
}

char *shmem_print(struct node *n) {
	struct shmem *shm = n->_vd;
	char *buf = NULL;
	strcatf(&buf, "name=%s, insize=%d, outsize=%d, sample_size=%d", shm->name, shm->insize, shm->outsize, shm->sample_size);
	return buf;
};

static struct node_type vt = {
	.name = "shmem",
	.description = "use POSIX shared memory to interface with other programs",
	.vectorize = 1,
	.size = sizeof(struct shmem),
	.parse = shmem_parse,
	.print = shmem_print,
	.open = shmem_open,
	.close = shmem_close,
	.read = shmem_read,
	.write = shmem_write
};

REGISTER_NODE_TYPE(&vt)

size_t shmem_total_size(int insize, int outsize, int sample_size)
{
	// we have the constant const of the memtype header
	return sizeof(struct memtype)
		// and the shared struct itself
		+ sizeof(struct shmem_shared)
		// the size of the 2 queues and the queue for the pool
		+ (insize + outsize) * (2*sizeof(struct queue_cell))
		// the size of the pool
		+ (insize + outsize) * kernel_get_cacheline_size() * CEIL(SAMPLE_LEN(sample_size), kernel_get_cacheline_size())
		// a memblock for each allocation (1 shmem_shared, 3 queues, 1 pool)
		+ 5 * sizeof(struct memblock)
		// and some extra buffer for alignment
		+ 1024;
}

struct shmem_shared* shmem_int_open(const char *name, size_t len)
{
	int fd = shm_open(name, O_RDWR, 0);
	if (fd < 0)
		return NULL;
	void *base = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (base == MAP_FAILED)
		return NULL;
	/* This relies on the behaviour of the node and the allocator; it assumes
	 * that memtype_managed is used and the shmem_shared is the first allocated object */
	char *cptr = (char *) base + sizeof(struct memtype) + sizeof(struct memblock);
	return (struct shmem_shared *) cptr;
}
