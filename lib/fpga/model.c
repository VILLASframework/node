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

int model_init(struct model *m, uintptr_t baseaddr)
{
	int ret;

	/** @todo: Currently only XSG models are supported */

	list_init(&m->parameters);
	list_init(&m->infos);

	ret = model_init_from_xsg_map(m, baseaddr);
	if (ret)
		error("Failed to init XSG model: %d", ret);

	return 0;
}

static void model_param_destroy(struct model_param *p)
{
	free(p->name);
}

static void model_info_destroy(struct model_info *i)
{
	free(i->field);
	free(i->value);
}

void model_destroy(struct model *m)
{
	list_destroy(&m->parameters, (dtor_cb_t) model_param_destroy, true);
	list_destroy(&m->infos, (dtor_cb_t) model_info_destroy, true);

	free(m->map);
}

void model_dump(struct model *m)
{
	const char *param_type[] = { "UFix", "Fix", "Float", "Boolean" };
	const char *model_types[] = { "HLS", "XSG" };
	const char *parameter_dirs[] = { "In", "Out", "In/Out" };

	info("Model: (type=%s)", model_types[m->type]);
	
	{ INDENT
		info("Parameters:");
		list_foreach(struct model_param *p, &m->parameters) { INDENT
			if (p->direction == MODEL_PARAM_IN)
				info("%#jx: %s (%s) = %.3f %s %u",
					p->offset,
					p->name,
					parameter_dirs[p->direction],
					p->default_value.flt,
					param_type[p->type],
					p->binpt
				);
			else if (p->direction == MODEL_PARAM_OUT)
				info("%#jx: %s (%s)",
					p->offset,
					p->name,
					parameter_dirs[p->direction]
				);
		}

		info("Infos:");
		list_foreach(struct model_info *i, &m->infos) { INDENT
			info("%s: %s", i->field, i->value);
		}
	}
}

int model_param_read(struct model_param *p, double *v)
{
	union model_param_value *ptr = p->offset;// TODO: + p->model->baseaddr;

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
	union model_param_value *ptr = p->offset;// TODO: + p->model->baseaddr;

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

int model_init_from_xsg_map(struct model *m, uintptr_t baseaddr)
{
	int ret;

	list_init(&m->infos);

	m->map = alloc(1024);
	m->maplen = model_xsg_map_read(baseaddr, m->map, 1024 / 4);
	if (m->maplen < 0)
		return -1;

	debug(5, "XSG: memory map length = %#zx", m->maplen);

	ret = model_xsg_map_parse((uint32_t *) m->map, m->maplen, &m->parameters, &m->infos);
	if (ret)
		return -2;

	return 0;
}

static uint32_t model_xsg_map_checksum(uint32_t *map, size_t len)
{
	uint32_t chks = 0;
	for (int i = 2; i < len-1; i++)
		chks += map[i];

	return chks; /* moduluo 2^32 because of overflow */
}

int model_xsg_map_parse(uint32_t *map, size_t len, struct list *parameters, struct list *infos)
{
#define copy_string(off) strndup((char *) (data + (off)), (length - (off)) * 4);

	struct model_param *p;
	struct model_info *i;
	int j;
	
	if (map[0] != XSG_MAGIC)
		error("Invalid magic: %#x", map[0]);

	for (j = 2; j < len-1;) {
		uint16_t type   = map[j] & 0xFFFF;
		uint16_t length = map[j] >> 16;
		uint32_t *data  = &map[j+1];
		
		switch (type) {
			case XSG_BLOCK_GATEWAY_IN:
			case XSG_BLOCK_GATEWAY_OUT:
				if (length < 4)
					break; /* block is to small to describe a gateway */
			
				p = alloc(sizeof(*p));

				p->name          =  copy_string(3);
				p->default_value.flt =  *((float *) &data[1]);
				p->offset        = data[2];
				p->direction     = type & 0x1;
				p->type          = (data[0] >> 0) & 0xFF;
				p->binpt         = (data[0] >> 8) & 0xFF;

				list_push(parameters, p);
				break;

			case XSG_BLOCK_INFO:
				i = alloc(sizeof(struct model_info));

				i->field = copy_string(0);
				i->value = copy_string((int) ceil((double) (strlen(i->field) + 1) / 4))
				
				list_push(infos, i);
				break;

			default:
				warn("Unknown block type: %#06x", type);
		}
		
		j += length + 1;
	}
	
	if (model_xsg_map_checksum(map, len) != map[j])
		error("XSG: Invalid checksum");

	return 0;

#undef copy_string
}

int model_xsg_map_read(void *baseaddr, void *dst, size_t len)
{
#define get_word(a) ({ *addr = a; *data; })

	volatile uint32_t *addr = baseaddr + 0x00;
	volatile uint32_t *data = baseaddr + 0x04;

	uint32_t *map = dst;
	uint32_t  maplen, magic;

	/* Check start DW */
	magic = get_word(0);
	if (magic != XSG_MAGIC)
		return -1;

	maplen = get_word(1);
	if (maplen < 3)
		return -2;

	/* Read Data */
	int i;
	for (i = 0; i < MIN(maplen, len); i++)
		map[i] = get_word(i);

	return i;

#undef get_word
}