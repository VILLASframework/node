/** Node type: Wrapper around RSCAD CBuilder model
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 **********************************************************************************/

#include <sys/eventfd.h>

#include <villas/node.h>
#include <villas/log.h>
#include <villas/plugin.h>
#include <villas/utils.h>

#include <villas/nodes/cbuilder.h>

int cbuilder_parse(struct node *n, json_t *cfg)
{
	struct cbuilder *cb = (struct cbuilder *) n->_vd;
	json_t *json_param, *json_params = NULL;

	const char *model;

	int ret;
	size_t index;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: F, s: s, s: b }",
		"timestep", &cb->timestep,
		"model", &model,
		"parameters", &json_params
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	cb->model = (struct cbuilder_model *) plugin_lookup(PLUGIN_TYPE_MODEL_CBUILDER, model);
	if (!cb->model)
		error("Unknown model '%s' of node %s", model, node_name(n));

	if (json_params) {
		if (!json_is_array(json_params))
			error("Setting 'parameters' of node %s must be an JSON array of numbers!", node_name(n));

		cb->paramlen = json_array_size(json_params);
		cb->params = alloc(cb->paramlen * sizeof(double));

		json_array_foreach(json_params, index, json_param) {
			if (json_is_number(json_param))
				error("Setting 'parameters' of node %s must be an JSON array of numbers!", node_name(n));

			cb->params[index] = json_number_value(json_params);

		}
	}

	return 0;
}

int cbuilder_start(struct node *n)
{
	int ret;
	struct cbuilder *cb = (struct cbuilder *) n->_vd;

	/* Initialize mutex and cv */
	pthread_mutex_init(&cb->mtx, NULL);

	cb->eventfd = eventfd(0, 0);
	if (cb->eventfd < 0)
		return -1;

	/* Currently only a single timestep per model / instance is supported */
	cb->step = 0;
	cb->read = 0;

	ret = cb->model->init(cb);
	if (ret)
		error("Failed to intialize CBuilder model %s", node_name(n));

	cb->model->ram();

	return 0;
}

int cbuilder_stop(struct node *n)
{
	int ret;
	struct cbuilder *cb = (struct cbuilder *) n->_vd;

	ret = close(cb->eventfd);
	if (ret)
		return ret;

	pthread_mutex_destroy(&cb->mtx);

	return 0;
}

int cbuilder_read(struct node *n, struct sample *smps[], int *cnt)
{
	struct cbuilder *cb = (struct cbuilder *) n->_vd;
	struct sample *smp = smps[0];

	uint64_t cntr;
	ssize_t bytes;

	bytes = read(cb->eventfd, &cntr, sizeof(cntr));
	if (bytes != sizeof(cntr))
		return -1;


	/* Wait for completion of step */
	pthread_mutex_lock(&cb->mtx);

	float data[smp->capacity];

	smp->length = cb->model->read(data, smp->capacity);

	/* Cast float -> double */
	for (int i = 0; i < smp->length; i++)
		smp->data[i].f = data[i];

	smp->sequence = cb->step;

	cb->read = cb->step;

	pthread_mutex_unlock(&cb->mtx);

	return 1;
}

int cbuilder_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct cbuilder *cb = (struct cbuilder *) n->_vd;
	struct sample *smp = smps[0];

	pthread_mutex_lock(&cb->mtx);

	float flt = smp->data[0].f;

	cb->model->write(&flt, smp->length);
	cb->model->code();

	cb->step++;

	uint64_t incr = 1;
	ssize_t bytes;

	bytes = write(cb->eventfd, &incr, sizeof(incr));

	pthread_mutex_unlock(&cb->mtx);

	if (bytes != sizeof(incr))
		return -1;

	return 1;
}

int cbuilder_fd(struct node *n)
{
	struct cbuilder *cb = (struct cbuilder *) n->_vd;

	return cb->eventfd;
}

static struct plugin p = {
	.name		= "cbuilder",
	.description	= "RTDS CBuilder model",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 1,
		.size		= sizeof(struct cbuilder),
		.parse		= cbuilder_parse,
		.start		= cbuilder_start,
		.stop		= cbuilder_stop,
		.read		= cbuilder_read,
		.write		= cbuilder_write,
		.fd		= cbuilder_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
