/** Interface to Xilinx System Generator Models via PCIe
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 *********************************************************************************/

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once

#include <stdlib.h>
#include <stdint.h>

#include "list.h"

#define XSG_MAPLEN		0x1000
#define XSG_MAGIC		0xDEADBABE

/* Forward declaration */
struct ip;

enum model_type {
	MODEL_TYPE_HLS,
	MODEL_TYPE_XSG
};

enum model_xsg_block_type {
	XSG_BLOCK_GATEWAY_IN	= 0x1000,
	XSG_BLOCK_GATEWAY_OUT	= 0x1001,
	XSG_BLOCK_INFO		= 0x2000
};

enum model_parameter_type {
	MODEL_PARAMETER_TYPE_UFIX,
	MODEL_PARAMETER_TYPE_FIX,
	MODEL_PARAMETER_TYPE_FLOAT,
	MODEL_PARAMETER_TYPE_BOOLEAN
};

enum model_parameter_direction {
	MODEL_PARAMETER_IN,
	MODEL_PARAMETER_OUT,
	MODEL_PARAMETER_INOUT
};

union model_parameter_value {
	uint32_t ufix;
	int32_t  fix;
	float    flt;
	bool     bol;
};

struct xsg_model {
	uint32_t *map;
	ssize_t maplen;
};

struct hls_model {

};

struct model {
	enum model_type type;		/**< Either HLS or XSG model */

	struct list parameters;		/**< List of model parameters. */
	struct list infos;		/**< A list of key / value pairs with model details */

	union {
		struct xsg_model xsg;	/**< XSG specific model data */
		struct hls_model hls;	/**< HLS specific model data */
	};
};

struct model_info {
	char *field;
	char *value;
};

struct model_parameter {
	char *name;				/**< Name of the parameter */

	enum model_parameter_direction direction;	/**< Read / Write / Read-write? */
	enum model_parameter_type type;		/**< Data type. Integers are represented by MODEL_GW_TYPE_(U)FIX with model_gw::binpt == 0 */

	int binpt;				/**< Binary point for type == MODEL_GW_TYPE_(U)FIX */
	uintptr_t offset;			/**< Register offset to model::baseaddress */

	union model_parameter_value default_value;

	struct fpga_ip *ip;				/**< A pointer to the model structure to which this parameters belongs to. */
};

/** Initialize a model */
int model_init(struct fpga_ip *c);

/** Parse model */
int model_parse(struct fpga_ip *c, json_t *cfg);

/** Destroy a model */
int model_destroy(struct fpga_ip *c);

/** Print detailed information about the model to the screen. */
void model_dump(struct fpga_ip *c);

/** Add a new parameter to the model */
void model_parameter_add(struct fpga_ip *c, const char *name, enum model_parameter_direction dir, enum model_parameter_type type);

/** Remove an existing parameter by its name */
int model_parameter_remove(struct fpga_ip *c, const char *name);

/** Read a model parameter.
 *
 * Note: the data type of the register is taken into account.
 * All datatypes are converted to double.
 */
int model_parameter_read(struct model_parameter *p, double *v);

/** Update a model parameter.
 *
 * Note: the data type of the register is taken into account.
 * The double argument will be converted to the respective data type of the
 * GatewayIn/Out block.
 */
int model_parameter_write(struct model_parameter *p, double v);

int model_parameter_update(struct model_parameter *p, struct model_parameter *u);

/** @} */
