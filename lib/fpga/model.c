/** Interface to Xilinx System Generator Models via PCIe
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdint.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include <villas/utils.h>
#include <villas/log.h>

#include "fpga/model.h"
#include "fpga/xsg.h"

int model_init(struct model *m, const char *xml)
{
	/** @todo: parse XML model descriptions */

	list_init(&m->parameters);

	return 0;
}

void model_destroy(struct model *m)
{
	list_destroy(&m->parameters, (dtor_cb_t) model_param_destroy, true);
}

void model_dump(struct model *m)
{
	const char *param_type[] = { "UFix", "Fix", "Float", "Boolean" };
	const char *model_types[] = { "HLS", "XSG" };
	const char *parameter_dirs[] = { "In", "Out", "In/Out" };

	info("Model: %s (baseaddr=%p, type=%s)", m->name, m->baseaddr, model_types[m->type]);
	
	{ INDENT
		info("Parameters:");
		list_foreach(struct model_param *p, &m->parameters) { INDENT
			if (p->direction == MODEL_PARAM_IN)
				info("%#x: %s (%s) = %.3f %s %u",
					p->offset,
					p->name,
					parameter_dirs[p->direction],
					p->default_value.flt,
					param_type[p->type],
					p->binpt
				);
			else if (p->direction == MODEL_PARAM_OUT)
				info("%#x: %s (%s)",
					p->offset,
					p->name,
					parameter_dirs[p->direction]
				);
		}

		switch (m->type) {
			case MODEL_TYPE_HLS:
				//hls_dump(&m->hls);
				break;
			case MODEL_TYPE_XSG:
				xsg_dump(&m->xsg);
				break;
		}
	}
}

int model_param_read(struct model_param *p, double *v)
{
	union model_param_value *ptr = p->offset + p->model->baseaddr;

	switch (p->type) {
		case MODEL_PARAM_TYPE_UFIX:
			*v = (double) ptr->ufix / (1 << p->binpt);
			break;

		case MODEL_PARAM_TYPE_FIX:
			*v = (double) ptr->fix / (1 << p->binpt);
			break;

		case MODEL_PARAM_TYPE_FLOAT:
			*v = (double) ptr->flt;
			break;
			
		case MODEL_PARAM_TYPE_BOOLEAN:
			*v = (double) ptr->ufix ? 1 : 0;
	}

	return 0;
}

int model_param_write(struct model_param *p, double v)
{
	union model_param_value *ptr = p->offset + p->model->baseaddr;

	switch (p->type) {
		case MODEL_PARAM_TYPE_UFIX:
			ptr->ufix = (uint32_t) (v * (1 << p->binpt));
			break;

		case MODEL_PARAM_TYPE_FIX:
			ptr->fix = (int32_t) (v * (1 << p->binpt));
			break;

		case MODEL_PARAM_TYPE_FLOAT:
			ptr->flt = (float) v;
			break;
			
		case MODEL_PARAM_TYPE_BOOLEAN:
			ptr->bol = (bool) v;
			break;
	}

	return 0;
}

void model_param_add(struct model *m, const char *name, enum model_param_direction dir, enum model_param_type type)
{
	struct model_param *p = alloc(sizeof(struct model_param));
	
	p->name = strdup(name);
	p->type = type;
	p->direction = dir;
	
	list_push(&m->parameters, p);
}

void model_param_destroy(struct model_param *p)
{
	free(p->name);
}
