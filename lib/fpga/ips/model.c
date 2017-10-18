/** Interface to Xilinx System Generator Models via PCIe
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 *********************************************************************************/

#include <stdint.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include "utils.h"
#include "log.h"
#include "log_config.h"
#include "plugin.h"

#include "fpga/ip.h"
#include "fpga/card.h"
#include "fpga/ips/model.h"

static int model_parameter_destroy(struct model_parameter *p)
{
	free(p->name);

	return 0;
}

static int model_info_destroy(struct model_info *i)
{
	free(i->field);
	free(i->value);

	return 0;
}

static uint32_t model_xsg_map_checksum(uint32_t *map, size_t len)
{
	uint32_t chks = 0;

	for (int i = 2; i < len-1; i++)
		chks += map[i];

	return chks; /* moduluo 2^32 because of overflow */
}

static int model_xsg_map_parse(uint32_t *map, size_t len, struct list *parameters, struct list *infos)
{
#define copy_string(off) strndup((char *) (data + (off)), (length - (off)) * 4);
	int j;
	struct model_info *i;

	/* Check magic */
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

				struct model_parameter *e, *p = (struct model_parameter *) alloc(sizeof(struct model_parameter));

				p->name          =  copy_string(3);
				p->default_value.flt =  *((float *) &data[1]);
				p->offset        = data[2];
				p->direction     = type & 0x1;
				p->type          = (data[0] >> 0) & 0xFF;
				p->binpt         = (data[0] >> 8) & 0xFF;

				e = list_lookup(parameters, p->name);
				if (e)
					model_parameter_update(e, p);
				else
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

	return 0;

#undef copy_string
}

static uint32_t model_xsg_map_read_word(uint32_t offset, void *baseaddr)
{
	volatile uint32_t *addr = baseaddr + 0x00;
	volatile uint32_t *data = baseaddr + 0x04;

	*addr = offset; /* Update addr reg */

	return *data; /* Read data reg */
}

static int model_xsg_map_read(uint32_t *map, size_t len, void *baseaddr)
{
	size_t maplen;
	uint32_t magic;

	/* Check magic */
	magic = model_xsg_map_read_word(0, baseaddr);
	if (magic != XSG_MAGIC)
		return -1;

	maplen = model_xsg_map_read_word(1, baseaddr);
	if (maplen < 3)
		return -2;

	/* Read Data */
	int i;
	for (i = 0; i < MIN(maplen, len); i++)
		map[i] = model_xsg_map_read_word(i, baseaddr);

	return i;
}

int model_parse(struct fpga_ip *c, json_t *cfg)
{
	struct model *m = (struct model *) c->_vd;

	int ret;

	json_t *json_params;
	json_error_t err;

	if      (strcmp(c->vlnv.library, "hls") == 0)
		m->type = MODEL_TYPE_HLS;
	else if (strcmp(c->vlnv.library, "sysgen") == 0)
		m->type = MODEL_TYPE_XSG;
	else
		error("Unsupported model type: %s", c->vlnv.library);

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: o }", "parameters", &json_params);
	if (ret)
		jerror(&err, "Failed to parse configuration of FPGA IP '%s'", c->name);

	if (json_params) {
		if (!json_is_object(json_params))
			error("Setting 'parameters' must be a JSON object");

		const char *name;
		json_t *value;
		json_object_foreach(json_params, name, value) {
			if (!json_is_real(value))
				error("Parameters of FPGA IP '%s' must be of type floating point", c->name);

			struct model_parameter *p = (struct model_parameter *) alloc(sizeof(struct model_parameter));

			p->name = strdup(name);
			p->default_value.flt = json_real_value(value);

			list_push(&m->parameters, p);
		}
	}

	return 0;
}

static int model_init_from_xsg_map(struct model *m, void *baseaddr)
{
	int ret, chks;

	if (baseaddr == (void *) -1)
		return -1;

	m->xsg.map = alloc(XSG_MAPLEN);
	m->xsg.maplen = model_xsg_map_read(m->xsg.map, XSG_MAPLEN, baseaddr);
	if (m->xsg.maplen < 0)
		return -1;

	debug(5, "XSG: memory map length = %#zx", m->xsg.maplen);

	chks = m->xsg.map[m->xsg.maplen - 1];
	if (chks != model_xsg_map_checksum(m->xsg.map, m->xsg.maplen))
		return -2;

	ret = model_xsg_map_parse(m->xsg.map, m->xsg.maplen, &m->parameters, &m->infos);
	if (ret)
		return -3;

	debug(5, "XSG: Parsed %zu parameters and %zu model infos", list_length(&m->parameters), list_length(&m->infos));

	return 0;
}

int model_init(struct fpga_ip *c)
{
	int ret;

	struct model *m = (struct model *) c->_vd;

	list_init(&m->parameters);
	list_init(&m->infos);

	if (!fpga_vlnv_cmp(&c->vlnv, &(struct fpga_vlnv) { NULL, "sysgen", NULL, NULL }))
		ret = model_init_from_xsg_map(m, c->card->map + c->baseaddr);
	else
		ret = 0;

	/* Set default values for parameters */
	for (size_t i = 0; i < list_length(&m->parameters); i++) {
		struct model_parameter *p = (struct model_parameter *) list_at(&m->parameters, i);

		p->ip = c;

		if (p->direction == MODEL_PARAMETER_IN) {
			model_parameter_write(p, p->default_value.flt);
			info("Set parameter '%s' updated to default value: %f", p->name, p->default_value.flt);
		}
	}

	if (ret)
		error("Failed to init XSG model: %d", ret);

	return 0;
}

int model_destroy(struct fpga_ip *c)
{
	struct model *m = (struct model *) c->_vd;

	list_destroy(&m->parameters, (dtor_cb_t) model_parameter_destroy, true);
	list_destroy(&m->infos, (dtor_cb_t) model_info_destroy, true);

	if (m->xsg.map != NULL)
		free(m->xsg.map);

	return 0;
}

void model_dump(struct fpga_ip *c)
{
	struct model *m = (struct model *) c->_vd;

	const char *param_type[] = { "UFix", "Fix", "Float", "Boolean" };
	const char *parameter_dirs[] = { "In", "Out", "In/Out" };

	{ INDENT
		info("Parameters:");
		for (size_t i = 0; i < list_length(&m->parameters); i++) { INDENT
			struct model_parameter *p = (struct model_parameter *) list_at(&m->parameters, i);

			if (p->direction == MODEL_PARAMETER_IN)
				info("%#jx: %s (%s) = %.3f %s %u",
					p->offset,
					p->name,
					parameter_dirs[p->direction],
					p->default_value.flt,
					param_type[p->type],
					p->binpt
				);
			else if (p->direction == MODEL_PARAMETER_OUT)
				info("%#jx: %s (%s)",
					p->offset,
					p->name,
					parameter_dirs[p->direction]
				);
		}

		info("Infos:");
		for (size_t j = 0; j < list_length(&m->infos); j++) { INDENT
			struct model_info *i = (struct model_info *) list_at(&m->infos, j);

			info("%s: %s", i->field, i->value);
		}
	}
}

int model_parameter_read(struct model_parameter *p, double *v)
{
	struct fpga_ip *c = p->ip;

	union model_parameter_value *ptr = (union model_parameter_value *) (c->card->map + c->baseaddr + p->offset);

	switch (p->type) {
		case MODEL_PARAMETER_TYPE_UFIX:
			*v = (double) ptr->ufix / (1 << p->binpt);
			break;

		case MODEL_PARAMETER_TYPE_FIX:
			*v = (double) ptr->fix / (1 << p->binpt);
			break;

		case MODEL_PARAMETER_TYPE_FLOAT:
			*v = (double) ptr->flt;
			break;

		case MODEL_PARAMETER_TYPE_BOOLEAN:
			*v = (double) ptr->ufix ? 1 : 0;
	}

	return 0;
}

int model_parameter_write(struct model_parameter *p, double v)
{
	struct fpga_ip *c = p->ip;

	union model_parameter_value *ptr = (union model_parameter_value *) (c->card->map + c->baseaddr + p->offset);

	switch (p->type) {
		case MODEL_PARAMETER_TYPE_UFIX:
			ptr->ufix = (uint32_t) (v * (1 << p->binpt));
			break;

		case MODEL_PARAMETER_TYPE_FIX:
			ptr->fix = (int32_t) (v * (1 << p->binpt));
			break;

		case MODEL_PARAMETER_TYPE_FLOAT:
			ptr->flt = (float) v;
			break;

		case MODEL_PARAMETER_TYPE_BOOLEAN:
			ptr->bol = (bool) v;
			break;
	}

	return 0;
}

void model_parameter_add(struct fpga_ip *c, const char *name, enum model_parameter_direction dir, enum model_parameter_type type)
{
	struct model *m = (struct model *) c->_vd;
	struct model_parameter *p = (struct model_parameter *) alloc(sizeof(struct model_parameter));

	p->name = strdup(name);
	p->type = type;
	p->direction = dir;

	list_push(&m->parameters, p);
}

int model_parameter_remove(struct fpga_ip *c, const char *name)
{
	struct model *m = (struct model *) c->_vd;
	struct model_parameter *p;

	p = list_lookup(&m->parameters, name);
	if (!p)
		return -1;

	list_remove(&m->parameters, p);

	return 0;
}

int model_parameter_update(struct model_parameter *p, struct model_parameter *u)
{
	if (strcmp(p->name, u->name) != 0)
		return -1;

	p->direction = u->direction;
	p->type = u->type;
	p->binpt = u->binpt;
	p->offset = u->offset;

	return 0;
}

static struct plugin p_hls = {
	.name		= "Xilinx High Level Synthesis (HLS) model",
	.description	= "",
	.type		= PLUGIN_TYPE_FPGA_IP,
	.ip		= {
		.vlnv	= { NULL, "hls", NULL, NULL },
		.type	= FPGA_IP_TYPE_MODEL,
		.init	= model_init,
		.destroy = model_destroy,
		.dump	= model_dump,
		.parse	= model_parse
	}
};

REGISTER_PLUGIN(&p_hls)

static struct plugin p_sysgen = {
	.name		= "Xilinx System Generator for DSP (XSG) model",
	.description	= "",
	.type		= PLUGIN_TYPE_FPGA_IP,
	.ip		= {
		.vlnv	= { NULL, "sysgen", NULL, NULL },
		.type	= FPGA_IP_TYPE_MODEL,
		.init	= model_init,
		.destroy = model_destroy,
		.dump	= model_dump,
		.parse	= model_parse,
		.size	= sizeof(struct model)
	}
};

REGISTER_PLUGIN(&p_sysgen)
