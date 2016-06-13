/** Interface to Xilinx System Generator Models via VILLASfpga
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#ifndef _XSG_H_
#define _XSG_H_

#define XSG_MAGIC		0xDEADBABE

/* Forward declaration */
struct model;

enum xsg_block_type {
	XSG_BLOCK_GATEWAY_IN	= 0x1000,
	XSG_BLOCK_GATEWAY_OUT	= 0x1001,
	XSG_BLOCK_INFO		= 0x2000
};

struct xsg_model {
	int version;

	char *map;
	size_t maplen;

	struct list infos;
};

struct xsg_info {
	char *field;
	char *value;
};

int xsg_init_from_map(struct model *m);

int xsg_init_from_xml(struct model *m, const char *xml);

void xsg_destroy(struct xsg_model *x);

void xsg_dump(struct xsg_model *x);

void xsg_info_destroy(struct xsg_info *i);

/** Read the XSG model information from ROM */
int xsg_map_read(void *offset, void *dst, size_t len);

/** Parse binary model information from ROM */
int xsg_map_parse(uint32_t *map, size_t len, struct list *parameters, struct list *infos);

#endif /* _XSG_H_ */