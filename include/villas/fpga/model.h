/** Interface to Xilinx System Generator Models via PCIe
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/
 
#ifndef _MODEL_H_
#define _MODEL_H_

#include <stdlib.h>
#include <inttypes.h>

#include "list.h"

#define XSG_MAGIC		0xDEADBABE

enum model_type {
	MODEL_TYPE_HLS,
	MODEL_TYPE_XSG
};

enum model_xsg_block_type {
	XSG_BLOCK_GATEWAY_IN	= 0x1000,
	XSG_BLOCK_GATEWAY_OUT	= 0x1001,
	XSG_BLOCK_INFO		= 0x2000
};

enum model_param_type {
	MODEL_PARAM_TYPE_UFIX,
	MODEL_PARAM_TYPE_FIX,
	MODEL_PARAM_TYPE_FLOAT,
	MODEL_PARAM_TYPE_BOOLEAN
};

enum model_param_direction {
	MODEL_PARAM_IN,
	MODEL_PARAM_OUT,
	MODEL_PARAM_INOUT
};

union model_param_value {
	uint32_t ufix;
	int32_t  fix;
	float    flt;
	bool     bol;
};

struct model {
	char *xml;			/**< Path to a XML file which describes this model */
	enum model_type type;
	struct list parameters;		/**< List of model parameters. */
	struct list infos;

	char *map;
	size_t maplen;

	union {
		struct xsg_model {
			char *map;
			size_t maplen;
		} xsg;			/**< XSG specific model data */
		struct hls_model {

		} hls;			/**< HLS specific model data */
	};
};

struct model_info {
	char *field;
	char *value;
};

struct model_param {
	char *name;				/**< Name of the parameter */

	enum model_param_direction direction;	/**< Read / Write / Read-write? */
	enum model_param_type type;		/**< Data type. Integers are represented by MODEL_GW_TYPE_(U)FIX with model_gw::binpt == 0 */

	int binpt;				/**< Binary point for type == MODEL_GW_TYPE_(U)FIX */
	uintptr_t offset;			/**< Register offset to model::baseaddress */

	union model_param_value default_value;

	struct model *model;			/**< A pointer to the model structure to which this parameters belongs to. */
};

int model_init(struct model *m, uintptr_t baseaddr);

int model_init_from_xsg_map(struct model *m, uintptr_t baseaddr);

int model_init_from_xml(struct model *m);

void model_destroy(struct model *m);

void model_dump(struct model *m);

void model_param_add(struct model *m, const char *name, enum model_param_direction dir, enum model_param_type type);

/** Read a model parameter.
 *
 * Note: the data type of the register is taken into account.
 * All datatypes are converted to double.
 */
int model_param_read(struct model_param *p, double *v);

/** Update a model parameter.
 *
 * Note: the data type of the register is taken into account.
 * The double argument will be converted to the respective data type of the
 * GatewayIn/Out block.
 */
int model_param_write(struct model_param *p, double v);

/** Read the XSG model information from ROM */
int model_xsg_map_read(void *offset, void *dst, size_t len);

/** Parse binary model information from ROM */
int model_xsg_map_parse(uint32_t *map, size_t len, struct list *parameters, struct list *infos);

#endif /* _MODEL_H_ */