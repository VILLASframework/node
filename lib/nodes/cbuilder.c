/** Node type: Wrapper around RSCAD CBuilder model
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include "node.h"
#include "log.h"
#include "nodes/cbuilder.h"

struct list cbmodels;	/**< Table of existing CBuilder models */

int cbuilder_parse(struct node *n, config_setting_t *cfg)
{
	struct cbuilder *cb = n->_vd;
	config_setting_t *cfg_params;

	const char *model;

	if (!config_setting_lookup_float(cfg, "timestep", &cb->timestep))
		cerror(cfg, "CBuilder model requires 'timestep' setting");
	
	if (!config_setting_lookup_string(cfg, "model", &model))
		cerror(cfg, "CBuilder model requires 'model' setting");
	
	cb->model = list_lookup(&cbmodels, model);
	if (!cb->model)
		cerror(cfg, "Unknown model '%s'", model);
	
	cfg_params = config_setting_get_member(cfg, "parameters");
	if (cfg_params) {
		if (!config_setting_is_array(cfg_params))
			cerror(cfg_params, "Model parameters must be an array of numbers!");

		cb->paramlen = config_setting_length(cfg_params);
		cb->params = alloc(cb->paramlen * sizeof(double));
		
		for (int i = 0; i < cb->paramlen; i++)
			cb->params[i] = config_setting_get_float_elem(cfg_params, i);
	}

	return 0;
}

int cbuilder_open(struct node *n)
{
	int ret;
	struct cbuilder *cb = n->_vd;

	/* Initialize mutex and cv */
	pthread_mutex_init(&cb->mtx, NULL);
	pthread_cond_init(&cb->cv, NULL);

	/* Currently only a single timestep per model / instance is supported */
	cb->step = 0;
	cb->read = 0;

	ret = cb->model->init(cb);
	if (ret)
		error("Failed to intialize CBuilder model %s", node_name(n));

	cb->model->ram();

	return 0;
}

int cbuilder_close(struct node *n)
{
	struct cbuilder *cb = n->_vd;
	
	pthread_mutex_destroy(&cb->mtx);
	pthread_cond_destroy(&cb->cv);

	return 0;
}

int cbuilder_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct cbuilder *cb = n->_vd;
	struct sample *smp = smps[0];

	/* Wait for completion of step */
	pthread_mutex_lock(&cb->mtx);
	while (cb->read >= cb->step)
		pthread_cond_wait(&cb->cv, &cb->mtx);

	smp->length = cb->model->read(&smp->data[0].f, 16);
	smp->sequence = cb->step;

	cb->read = cb->step;

	pthread_mutex_unlock(&cb->mtx);

	return 1;
}

int cbuilder_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct cbuilder *cb = n->_vd;
	struct sample *smp = smps[0];
	
	pthread_mutex_lock(&cb->mtx);

	cb->model->write(&smp->data[0].f, smp->length);
	cb->model->code();

	cb->step++;

	pthread_cond_signal(&cb->cv);
	pthread_mutex_unlock(&cb->mtx);

	return 1;
}

static struct node_type vt = {
	.name		= "cbuilder",
	.description	= "RTDS CBuilder model",
	.vectorize	= 1,
	.size		= sizeof(struct cbuilder),
	.parse		= cbuilder_parse,
	.open		= cbuilder_open,
	.close		= cbuilder_close,
	.read		= cbuilder_read,
	.write		= cbuilder_write,
};

REGISTER_NODE_TYPE(&vt)
