/** Interface to Xilinx System Generator Models via VILLASfpga
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>

#include <villas/list.h>
#include <villas/utils.h>
#include <villas/log.h>

#include "fpga/model.h"
#include "fpga/xsg.h"
#include "utils.h"

int xsg_init_from_map(struct model *m)
{
	int ret;
	struct xsg_model *x = &m->xsg;

	list_init(&x->infos);

	x->map = alloc(1024);

	x->maplen = xsg_map_read(m->baseaddr, x->map, 1024 / 4);
	if (x->maplen < 0)
		return -1;

	debug(5, "XSG: memory map length = %#zx", x->maplen);

	ret = xsg_map_parse((uint32_t *) x->map, x->maplen, &m->parameters, &x->infos);
	if (ret)
		return -2;

	m->name = xsg_get_info(x, "Name");

	return 0;
}

int xsg_init_from_xml(struct model *m, const char *xml)
{
	/** @todo: parse Simulink .mdl file here (XML based) */

	return -1;
}

void xsg_destroy(struct xsg_model *x)
{
	list_destroy(&x->infos, (dtor_cb_t) xsg_info_destroy, true);

	free(x->map);
}

char * xsg_get_info(struct xsg_model *x, const char *key)
{
	struct xsg_info *info = list_lookup(&x->infos, key);

	return info ? info->value : NULL;
}

void xsg_info_destroy(struct xsg_info *i)
{
	free(i->field);
	free(i->value);
}

void xsg_dump(struct xsg_model *x)
{
	info("XSG Infos:");
	list_foreach(struct xsg_info *i, &x->infos) { INDENT
		info("%s: %s", i->field, i->value);
	}
}

static uint32_t xsg_map_checksum(uint32_t *map, size_t len)
{
	uint32_t chks = 0;
	for (int i = 2; i < len-1; i++)
		chks += map[i];

	return chks; /* moduluo 2^32 because of overflow */
}

int xsg_map_parse(uint32_t *map, size_t len, struct list *parameters, struct list *infos)
{
#define copy_string(off) strndup((char *) (data + (off)), (length - (off)) * 4);

	struct model_param *p;
	struct xsg_info *info;
	
	int i;
	
	if (map[0] != XSG_MAGIC)
		error("Invalid magic: %#x", map[0]);

	for (i = 2; i < len-1;) {
		uint16_t type   = map[i] & 0xFFFF;
		uint16_t length = map[i] >> 16;
		uint32_t *data  = &map[i+1];
		
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
				info = alloc(sizeof(*info));

				info->field = copy_string(0);
				info->value = copy_string((int) ceil((double) (strlen(info->field) + 1) / 4))
				
				list_push(infos, info);
				break;

			default:
				warn("Unknown block type: %#06x", type);
		}
		
		i += length + 1;
	}
	
	if (xsg_map_checksum(map, len) != map[i])
		error("XSG: Invalid checksum");

	return 0;

#undef copy_string
}

int xsg_map_read(void *baseaddr, void *dst, size_t len)
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