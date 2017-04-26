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
#include "stats.h"

static void path_read(struct path *p)
{
	int recv;
	int enqueue;
	int enqueued;
	int ready; /**< Number of blocks in smps[] which are allocated and ready to be used by node_read(). */
	
	struct path_source *ps = p->source;
	
	int cnt = ps->node->vectorize;

	struct sample *smps[cnt];

	/* Fill smps[] free sample blocks from the pool */
	ready = sample_alloc(&ps->pool, smps, cnt);
	if (ready != cnt)
		warn("Pool underrun for path %s", path_name(p));

	/* Read ready samples and store them to blocks pointed by smps[] */
	recv = node_read(ps->node, smps, ready);
	if (recv < 0)
		error("Failed to receive message from node %s", node_name(ps->node));
	else if (recv < ready) {
		warn("Partial read for path %s: read=%u expected=%u", path_name(p), recv, ready);
		/* Free samples that weren't written to */
		sample_free(smps+recv, ready-recv);
	}

	debug(LOG_PATH | 15, "Received %u messages from node %s", recv, node_name(ps->node));

	/* Run preprocessing hooks for vector of samples */
	enqueue = hook_read_list(&p->hooks, smps, recv);
	if (enqueue != recv) {
		info("Hooks skipped %u out of %u samples for path %s", recv - enqueue, recv, path_name(p));

		if (p->stats)
			stats_update(p->stats->delta, STATS_SKIPPED, recv - enqueue);
	}

	/* Keep track of the lowest index that wasn't enqueued;
	 * all following samples must be freed here */
	int refd = 0;
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);
	
		enqueued = queue_push_many(&pd->queue, (void **) smps, enqueue);
		if (enqueue != enqueued)
			warn("Queue overrun for path %s", path_name(p));

		if (refd < enqueued)
			refd = enqueued;

		debug(LOG_PATH | 15, "Enqueued %u samples from %s to queue of %s", enqueued, node_name(ps->node), node_name(pd->node));
	}

	/* Release those samples which have not been pushed into a queue */
	sample_free(smps + refd, ready - refd);
}

static void path_write(struct path *p)
{
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);
	
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

			tosend = hook_write_list(&p->hooks, smps, available);
			if (tosend == 0)
				continue;

			sent = node_write(pd->node, smps, tosend);
			if (sent < 0)
				error("Failed to sent %u samples to node %s", cnt, node_name(pd->node));
			else if (sent < tosend)
				warn("Partial write to node %s", node_name(pd->node));

			debug(LOG_PATH | 15, "Sent %u messages to node %s", sent, node_name(pd->node));

			released = sample_put_many(smps, sent);
	
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
	
	/* Default values */
	p->reverse = 0;
	p->enabled = 1;

	p->samplelen = DEFAULT_SAMPLELEN;
	p->queuelen = DEFAULT_QUEUELEN;
	
	p->super_node = sn;
	
	p->state = STATE_INITIALIZED;
	
	return 0;
}

int path_parse(struct path *p, config_setting_t *cfg, struct list *nodes)
{
	config_setting_t *cfg_out, *cfg_hooks;
	const char *in;
	int ret;

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
	if (ret || list_length(&destinations) == 0)
		cerror(cfg_out, "Invalid output nodes");

	/* Optional settings */
	cfg_hooks = config_setting_get_member(cfg, "hooks");
	if (cfg_hooks) {
		ret = hook_parse_list(&p->hooks, cfg_hooks, p);
		if (ret)
			return ret;
	}

	config_setting_lookup_bool(cfg, "reverse", &p->reverse);
	config_setting_lookup_bool(cfg, "enabled", &p->enabled);
	config_setting_lookup_int(cfg, "samplelen", &p->samplelen);
	config_setting_lookup_int(cfg, "queuelen", &p->queuelen);

	if (!IS_POW2(p->queuelen)) {
		p->queuelen = LOG2_CEIL(p->queuelen);
		warn("Queue length should always be a power of 2. Adjusting to %d", p->queuelen);
	}

	p->cfg = cfg;

	p->source = alloc(sizeof(struct path_source));
	p->source->node = source;
	p->source->samplelen = p->samplelen;

	for (size_t i = 0; i < list_length(&destinations); i++) {
		struct node *n = list_at(&destinations, i);

		struct path_destination pd = {
			.node = n,
			.queuelen = p->queuelen
		};
		
		list_push(&p->destinations, memdup(&pd, sizeof(pd)));
	}

	list_destroy(&destinations, NULL, false);

	return 0;
}

int path_check(struct path *p)
{
	assert(p->state != STATE_DESTROYED);
	
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);
		
		if (!pd->node->_vt->write)
			error("Destiation node '%s' is not supported as a sink for path '%s'", node_name(pd->node), path_name(p));
	}

	if (!p->source->node->_vt->read)
		error("Source node '%s' is not supported as source for path '%s'", node_name(p->source->node), path_name(p));
	
	p->state = STATE_CHECKED;
	
	return 0;
}

int path_init2(struct path *p)
{
	int ret, max_queuelen = 0;

	assert(p->state == STATE_CHECKED);

	/* Add internal hooks if they are not already in the list*/
	for (size_t i = 0; i < list_length(&plugins); i++) {
		struct plugin *q = list_at(&plugins, i);
		
		if (q->type == PLUGIN_TYPE_HOOK) {
			struct hook h = { .state = STATE_DESTROYED };
			struct hook_type *vt = &q->hook;

			if (vt->builtin) {
				ret = hook_init(&h, vt, p);
				if (ret)
					return ret;

				list_push(&p->hooks, memdup(&h, sizeof(h)));
			}
		}
	}
	
	/* We sort the hooks according to their priority before starting the path */
	list_sort(&p->hooks, hook_cmp_priority);

	/* Initialize destinations */
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);

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

	info("Starting path %s: #hooks=%zu, #destinations=%zu", path_name(p), list_length(&p->hooks), list_length(&p->destinations));

	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = list_at(&p->hooks, i);

		ret = hook_start(h);
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
	
	if (p->state != STATE_STARTED)
		return 0;

	info("Stopping path: %s", path_name(p));

	pthread_cancel(p->tid);
	pthread_join(p->tid, NULL);

	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = list_at(&p->hooks, i);

		ret = hook_stop(h);
		if (ret)
			return ret;
	}

	p->state = STATE_STOPPED;

	return 0;
}

int path_destroy(struct path *p)
{
	if (p->state == STATE_DESTROYED)
		return 0;

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
			
			for (size_t i = 0; i < list_length(&p->destinations); i++) {
				struct path_destination *pd = list_at(&p->destinations, i);

				strcatf(&p->_name, " %s", node_name_short(pd->node));
			}
			
			strcatf(&p->_name, " ]");
		}
	}

	return p->_name;
}

int path_uses_node(struct path *p, struct node *n) {
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);

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

	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = list_at(&p->hooks, i);
		struct hook hc = { .state = STATE_DESTROYED };
		
		ret = hook_init(&hc, h->_vt, p);
		if (ret)
			return ret;
		
		list_push(&r->hooks, memdup(&hc, sizeof(hc)));
	}
	
	return 0;
}
