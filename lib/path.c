/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <libconfig.h>

#include "config.h"
#include "utils.h"
#include "path.h"
#include "timing.h"
#include "pool.h"
#include "queue.h"
#include "hook.h"
#include "plugin.h"
#include "super_node.h"
#include "memory.h"

static void path_read(struct path *p)
{
	int recv;
	int enqueue;
	int enqueued;
	int ready = 0; /**< Number of blocks in smps[] which are allocated and ready to be used by node_read(). */
	
	struct path_source *ps = p->source;
	
	int cnt = ps->node->vectorize;

	struct sample *smps[cnt];

	/* Fill smps[] free sample blocks from the pool */
	ready += sample_alloc(&ps->pool, smps, cnt - ready);
	if (ready != cnt)
		warn("Pool underrun for path %s", path_name(p));

	/* Read ready samples and store them to blocks pointed by smps[] */
	recv = node_read(ps->node, smps, ready);
	if (recv < 0)
		error("Failed to receive message from node %s", node_name(ps->node));
	else if (recv < ready)
		warn("Partial read for path %s: read=%u expected=%u", path_name(p), recv, ready);

	debug(LOG_PATH | 15, "Received %u messages from node %s", recv, node_name(ps->node));

	/* Run preprocessing hooks for vector of samples */
	enqueue = path_run_hooks(p, HOOK_READ, smps, recv);
	if (enqueue != recv) {
		info("Hooks skipped %u out of %u samples for path %s", recv - enqueue, recv, path_name(p));
		
		stats_update(p->stats->delta, STATS_SKIPPED, recv - enqueue);
	}

	list_foreach(struct path_destination *pd, &p->destinations) {
		enqueued = queue_push_many(&pd->queue, (void **) smps, enqueue);
		if (enqueue != enqueued)
			warn("Queue overrun for path %s", path_name(p));
		
		for (int i = 0; i < enqueued; i++)
			sample_get(smps[i]); /* increase reference count */

		debug(LOG_PATH | 15, "Enqueued %u samples from %s to queue of %s", enqueued, node_name(ps->node), node_name(pd->node));
	}
}

static void path_write(struct path *p)
{
	list_foreach(struct path_destination *pd, &p->destinations) {
		int cnt = pd->node->vectorize;
		int sent;
		int tosend;
		int available;
		int released;

		struct sample *smps[cnt];

		/* As long as there are still samples in the queue */
		while (1) {
			available = queue_pull_many(&pd->queue, (void **) smps, cnt);
			if (available == 0)
				break;
			else if (available < cnt) 
				debug(LOG_PATH | 5, "Queue underrun for path %s: available=%u expected=%u", path_name(p), available, cnt);
			
			debug(LOG_PATH | 15, "Dequeued %u samples from queue of node %s which is part of path %s", available, node_name(pd->node), path_name(p));

			tosend = path_run_hooks(p, HOOK_WRITE, smps, available);
			if (tosend == 0)
				continue;

			sent = node_write(pd->node, smps, tosend);
			if (sent < 0)
				error("Failed to sent %u samples to node %s", cnt, node_name(pd->node));
			else if (sent < tosend)
				warn("Partial write to node %s", node_name(pd->node));

			debug(LOG_PATH | 15, "Sent %u messages to node %s", sent, node_name(pd->node));

			released = 0;
			for (int i = 0; i < sent; i++) {
				if (sample_put(smps[i]) == 0)
					released++; /* we had the last reference (0 remaining) */
			}
	
			debug(LOG_PATH | 15, "Released %d samples back to memory pool", released);
		}
	}
}

/** Main thread function per path: receive -> sent messages */
static void * path_run(void *arg)
{
	struct path *p = arg;

	for (;;) {
		path_read(p);
		path_write(p);
	}

	return NULL;
}

static int path_source_destroy(struct path_source *ps)
{
	pool_destroy(&ps->pool);
	
	return 0;
}

static int path_destination_destroy(struct path_destination *pd)
{
	queue_destroy(&pd->queue);
	
	return 0;
}

int path_init(struct path *p, struct super_node *sn)
{
	assert(p->state == STATE_DESTROYED);

	list_init(&p->hooks);
	list_init(&p->destinations);
	
	p->super_node = sn;
	
	p->state = STATE_INITIALIZED;
	
	return 0;
}

int path_parse(struct path *p, config_setting_t *cfg, struct list *nodes)
{
	config_setting_t *cfg_out, *cfg_hook;
	const char *in;
	int ret, samplelen, queuelen;

	struct node *source;
	struct list destinations = { .state = STATE_DESTROYED };
	
	list_init(&destinations);

	/* Input node */
	if (!config_setting_lookup_string(cfg, "in", &in) &&
	    !config_setting_lookup_string(cfg, "from", &in) &&
	    !config_setting_lookup_string(cfg, "src", &in) &&
	    !config_setting_lookup_string(cfg, "source", &in))
		cerror(cfg, "Missing input node for path");

	source = list_lookup(nodes, in);
	if (!source)
		cerror(cfg, "Invalid input node '%s'", in);

	/* Output node(s) */
	if (!(cfg_out = config_setting_get_member(cfg, "out")) &&
	    !(cfg_out = config_setting_get_member(cfg, "to")) &&
	    !(cfg_out = config_setting_get_member(cfg, "dst")) &&
	    !(cfg_out = config_setting_get_member(cfg, "dest")) &&
	    !(cfg_out = config_setting_get_member(cfg, "sink")))
		cerror(cfg, "Missing output nodes for path");

	ret = node_parse_list(&destinations, cfg_out, nodes);
	if (ret <= 0)
		cerror(cfg_out, "Invalid output nodes");

	/* Optional settings */
	cfg_hook = config_setting_get_member(cfg, "hook");
	if (cfg_hook)
		hook_parse_list(&p->hooks, cfg_hook);

	if (!config_setting_lookup_bool(cfg, "reverse", &p->reverse))
		p->reverse = 0;
	if (!config_setting_lookup_bool(cfg, "enabled", &p->enabled))
		p->enabled = 1;
	if (!config_setting_lookup_int(cfg, "values", &samplelen))
		samplelen = DEFAULT_VALUES;
	if (!config_setting_lookup_int(cfg, "queuelen", &queuelen))
		queuelen = DEFAULT_QUEUELEN;

	if (!IS_POW2(queuelen)) {
		queuelen = LOG2_CEIL(queuelen);
		warn("Queue length should always be a power of 2. Adjusting to %d", queuelen);
	}

	p->cfg = cfg;
	
	/* Check if nodes are suitable */
	if (source->_vt->read == NULL)
		cerror(cfg, "Input node '%s' is not supported as a source.", node_name(source));

	p->source = alloc(sizeof(struct path_source));
	p->source->node = source;
	p->source->samplelen = samplelen;

	list_foreach(struct node *n, &destinations) {
		if (n->_vt->write == NULL)
			cerror(cfg_out, "Output node '%s' is not supported as a destination.", node_name(n));
		
		struct path_destination pd;
		
		pd.node = n;
		pd.queuelen = queuelen;
		
		list_push(&p->destinations, memdup(&pd, sizeof(pd)));
	}

	list_destroy(&destinations, NULL, false);

	return 0;
}

int path_check(struct path *p)
{
	assert(p->state != STATE_DESTROYED);
	
	list_foreach(struct node *n, &p->destinations) {
		if (!n->_vt->write)
			error("Destiation node '%s' is not supported as a sink for path '%s'", node_name(n), path_name(p));
	}

	if (!p->source->node->_vt->read)
		error("Source node '%s' is not supported as source for path '%s'", node_name(p->source->node), path_name(p));
	
	return 0;
}

int path_init2(struct path *p)
{
	int ret, max_queuelen = 0;

	assert(p->state == STATE_CHECKED);

	/* Add internal hooks if they are not already in the list*/
	list_foreach(struct plugin *q, &plugins) {
		if (q->type == PLUGIN_TYPE_HOOK) {
			struct hook h;
			struct hook_type *vt = &q->hook;

			if (vt->when & HOOK_AUTO) {
				hook_init(&h, vt, p->super_node);

				list_push(&p->hooks, memdup(&h, sizeof(h)));
			}
		}
	}
	
	/* We sort the hooks according to their priority before starting the path */
	list_sort(&p->hooks, hook_cmp_priority);

	/* Initialize destinations */
	list_foreach(struct path_destination *pd, &p->destinations) {
		ret = queue_init(&pd->queue, pd->queuelen, &memtype_hugepage);
		if (ret)
			error("Failed to initialize queue for path");
		
		if (pd->queuelen > max_queuelen)
			max_queuelen = pd->queuelen;
	}
	
	/* Initialize source */
	ret = pool_init(&p->source->pool, max_queuelen, SAMPLE_LEN(p->source->samplelen), &memtype_hugepage);
	if (ret)
		error("Failed to allocate memory pool for path");
	
	return 0;
}

int path_start(struct path *p)
{
	int ret;
	
	assert(p->state == STATE_CHECKED);

	info("Starting path: %s (#hooks=%zu)", path_name(p), list_length(&p->hooks));

	list_foreach(struct hook *h, &p->hooks) {
		ret = hook_run(h, HOOK_PATH_START, &(struct hook_info) { .path = p });
		if (ret)
			return ret;
	}

	/* Start one thread per path for sending to destinations */
	ret = pthread_create(&p->tid, NULL, &path_run,  p);
	if (ret)
		return ret;

	p->state = STATE_STARTED;

	return 0;
}


int path_stop(struct path *p)
{
	int ret;

	info("Stopping path: %s", path_name(p));

	pthread_cancel(p->tid);
	pthread_join(p->tid, NULL);

	list_foreach(struct hook *h, &p->hooks) {
		ret = hook_run(h, HOOK_PATH_STOP, &(struct hook_info) { .path = p });
		if (ret)
			return ret;
	}

	p->state = STATE_STOPPED;

	return 0;
}

int path_destroy(struct path *p)
{
	list_destroy(&p->hooks, (dtor_cb_t) hook_destroy, true);
	list_destroy(&p->destinations, (dtor_cb_t) path_destination_destroy, true);
	
	path_source_destroy(p->source);

	if (p->_name)
		free(p->_name);

	if (p->source)
		free(p->source);
	
	p->state = STATE_DESTROYED;
	
	return 0;
}

const char * path_name(struct path *p)
{
	if (!p->_name) {
		if (list_length(&p->destinations) == 1) {
			struct path_destination *pd = (struct path_destination *) list_first(&p->destinations);
			
			strcatf(&p->_name, "%s " MAG("=>") " %s",
				node_name_short(p->source->node),
				node_name_short(pd->node));
		}
		else {
			strcatf(&p->_name, "%s " MAG("=>") " [", node_name_short(p->source->node));
			
			list_foreach(struct path_destination *pd, &p->destinations)
				strcatf(&p->_name, " %s", node_name_short(pd->node));
			
			strcatf(&p->_name, " ]");
		}
	}

	return p->_name;
}

int path_uses_node(struct path *p, struct node *n) {
	list_foreach(struct path_destination *pd, &p->destinations) {
		if (pd->node == n)
			return 0;
	}

	return p->source->node == n ? 0 : -1;
}

int path_reverse(struct path *p, struct path *r)
{
	int ret;

	if (list_length(&p->destinations) > 1)
		return -1;
	
	struct path_destination *first_pd = list_first(&p->destinations);

	/* General */
	r->enabled = p->enabled;
	r->cfg = p->cfg;
	
	struct path_destination *pd = alloc(sizeof(struct path_destination));
	
	pd->node = p->source->node;
	pd->queuelen = first_pd->queuelen;

	list_push(&r->destinations, pd);
	
	struct path_source *ps = alloc(sizeof(struct path_source));

	ps->node = first_pd->node;
	ps->samplelen = p->source->samplelen;
	
	r->source = ps;

	list_foreach(struct hook *h, &p->hooks) {
		struct hook hc;
		
		ret = hook_init(&hc, h->_vt, p->super_node);
		if (ret)
			return ret;
		
		list_push(&r->hooks, memdup(&hc, sizeof(hc)));
	}
	
	return 0;
}

int path_run_hooks(struct path *p, int when, struct sample *smps[], size_t cnt)
{
	int ret = 0;
	
	struct hook_info i = {
		.samples = smps,
		.count = cnt
	};

	list_foreach(struct hook *h, &p->hooks) {
		ret = hook_run(h, when, &i);
		if (ret)
			break;
		
		if (i.count == 0)
			break;
	}

	return i.count;
}