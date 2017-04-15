/** Node-type for shared memory communication.
 *
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kernel/kernel.h"
#include "log.h"
#include "nodes/shmem.h"
#include "plugin.h"
#include "shmem.h"
#include "utils.h"

int shmem_parse(struct node *n, config_setting_t *cfg)
{
	struct shmem *shm = n->_vd;

	if (!config_setting_lookup_string(cfg, "name", &shm->name))
		cerror(cfg, "Missing shared memory object name");
	if (!config_setting_lookup_int(cfg, "queuelen", &shm->queuelen))
		shm->queuelen = DEFAULT_SHMEM_QUEUELEN;
	if (!config_setting_lookup_int(cfg, "samplelen", &shm->samplelen))
		shm->samplelen = DEFAULT_SHMEM_SAMPLELEN;
	if (!config_setting_lookup_bool(cfg, "polling", &shm->polling))
		shm->polling = false;

	config_setting_t *exec_cfg = config_setting_lookup(cfg, "exec");
	if (!exec_cfg)
		shm->exec = NULL;
	else {
		if (!config_setting_is_array(exec_cfg))
			cerror(exec_cfg, "Invalid format for exec");

		shm->exec = alloc(sizeof(char *) * (config_setting_length(exec_cfg) + 1));

		int i;
		for (i = 0; i < config_setting_length(exec_cfg); i++) {
			const char *elm = config_setting_get_string_elem(exec_cfg, i);
			if (!elm)
				cerror(exec_cfg, "Invalid format for exec");
			
			shm->exec[i] = strdup(elm);
		}

		shm->exec[i] = NULL;
	}

	return 0;
}

int shmem_open(struct node *n)
{
	struct shmem *shm = n->_vd;
	int ret;
	size_t len;
	
	shm->fd = shm_open(shm->name, O_RDWR | O_CREAT, 0600);
	if (shm->fd < 0)
		serror("Opening shared memory object failed");
	
	len = shmem_total_size(shm->queuelen, shm->queuelen, shm->samplelen);
	
	ret = ftruncate(shm->fd, len);
	if (ret < 0)
		serror("Setting size of shared memory object failed");

	shm->base = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
	if (shm->base == MAP_FAILED)
		serror("Mapping shared memory failed");

	shm->manager = memtype_managed_init(shm->base, len);
	shm->shared = memory_alloc(shm->manager, sizeof(struct shmem_shared));
	if (!shm->shared)
		error("Shared memory shared struct allocation failed (not enough memory?)");

	memset(shm->shared, 0, sizeof(struct shmem_shared));
	shm->shared->len = len;
	shm->shared->polling = shm->polling;

	ret = shm->polling ? queue_init(&shm->shared->in.q, shm->queuelen, shm->manager)
			   : queue_signalled_init(&shm->shared->in.qs, shm->queuelen, shm->manager);
	if (ret)
		error("Shared memory queue allocation failed (not enough memory?)");

	ret = shm->polling ? queue_init(&shm->shared->out.q, shm->queuelen, shm->manager)
			   : queue_signalled_init(&shm->shared->out.qs, shm->queuelen, shm->manager);
	if (ret)
		error("Shared memory queue allocation failed (not enough memory?)");

	ret = pool_init(&shm->shared->pool, 2 * shm->queuelen, SAMPLE_LEN(shm->samplelen), shm->manager);
	if (ret)
		error("Shared memory pool allocation failed (not enough memory?)");

	pthread_barrierattr_init(&shm->shared->start_attr);
	pthread_barrierattr_setpshared(&shm->shared->start_attr, PTHREAD_PROCESS_SHARED);
	pthread_barrier_init(&shm->shared->start_bar, &shm->shared->start_attr, 2);

	if (shm->exec) {
		ret = spawn(shm->exec[0], shm->exec);
		if (!ret)
			serror("Failed to spawn external program");
	}

	pthread_barrier_wait(&shm->shared->start_bar);

	return 0;
}

int shmem_close(struct node *n)
{
	struct shmem* shm = n->_vd;
	int ret;

	atomic_store_explicit(&shm->shared->node_stopped, 1, memory_order_relaxed);

	if (!shm->polling) {
		pthread_mutex_lock(&shm->shared->out.qs.mt);
		pthread_cond_broadcast(&shm->shared->out.qs.ready);
		pthread_mutex_unlock(&shm->shared->out.qs.mt);
	}

	/* Don't destroy the data structures yet, since the other process might
	 * still be using them. Once both processes are done and have unmapped the
	 * memory, it will be freed anyway. */
	ret = munmap(shm->base, shm->shared->len);
	if (ret != 0)
		return ret;

	return shm_unlink(shm->name);
}

int shmem_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct shmem *shm = n->_vd;

	int ret, recv;

	recv = shm->polling ? queue_pull_many(&shm->shared->in.q, (void**) smps, cnt)
			   : queue_signalled_pull_many(&shm->shared->in.qs, (void**) smps, cnt);
	
	if (recv <= 0)
		return recv;
	
	/* Check if remote process is still running */
	ret = atomic_load_explicit(&shm->shared->ext_stopped, memory_order_relaxed);
	if (ret)
		return ret;

	return recv;
}

int shmem_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct shmem *shm = n->_vd;
	struct sample *shared_smps[cnt]; /**< Samples need to be copied to the shared pool first */
	int avail, pushed, len;
	
	avail = sample_alloc(&shm->shared->pool, shared_smps, cnt);
	if (avail != cnt)
		warn("Pool underrun for shmem node %s", shm->name);

	for (int i = 0; i < avail; i++) {
		/* Since the node isn't in shared memory, the source can't be accessed */
		shared_smps[i]->source = NULL;
		shared_smps[i]->sequence = smps[i]->sequence;
		shared_smps[i]->ts = smps[i]->ts;
		
		len = MIN(smps[i]->length, shared_smps[i]->capacity);
		if (len != smps[i]->length)
			warn("Losing data because of sample capacity mismatch in node %s", node_name(n));
		
		memcpy(shared_smps[i]->data, smps[i]->data, SAMPLE_DATA_LEN(len));
		
		shared_smps[i]->length = len;
		
		sample_get(shared_smps[i]);
	}

	if (atomic_load_explicit(&shm->shared->ext_stopped, memory_order_relaxed)) {
		for (int i = 0; i < avail; i++)
			sample_put(shared_smps[i]);

		return -1;
	}
	
	pushed = shm->polling ? queue_push_many(&shm->shared->out.q, (void**) shared_smps, avail)
			      : queue_signalled_push_many(&shm->shared->out.qs, (void**) shared_smps, avail);

	if (pushed != avail)
		warn("Outgoing queue overrun for node %s", node_name(n));

	return pushed;
}

char * shmem_print(struct node *n)
{
	struct shmem *shm = n->_vd;
	char *buf = NULL;

	strcatf(&buf, "name=%s, queuelen=%d, samplelen=%d, polling=%s",
		shm->name, shm->queuelen, shm->samplelen, shm->polling ? "yes" : "no");
	
	if (shm->exec) {
		strcatf(&buf, ", exec='");
		
		for (int i = 0; shm->exec[i]; i++)
			strcatf(&buf, shm->exec[i+1] ? "%s " : "%s", shm->exec[i]);
		
		strcatf(&buf, "'");
	}

	return buf;
};

static struct plugin p = {
	.name = "shmem",
	.description = "POSIX shared memory interface with external processes",
	.type = PLUGIN_TYPE_NODE,
	.node = {
		.vectorize = 0,
		.size = sizeof(struct shmem),
		.parse = shmem_parse,
		.print = shmem_print,
		.start = shmem_open,
		.stop = shmem_close,
		.read = shmem_read,
		.write = shmem_write,
		.instances = LIST_INIT(),
	}
};

REGISTER_PLUGIN(&p)
