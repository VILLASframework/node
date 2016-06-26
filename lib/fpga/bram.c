/** Block RAM
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include "fpga/ip.h"
#include "fpga/timer.h"

int bram_parse(struct ip *c)
{
	int size;

	if (config_setting_lookup_int(c->cfg, "size", &size))
		c->bram.size = size;
	else
		cerror(c->cfg, "BRAM IP core requires 'size' setting");

	if (size <= 0)
		error("BRAM IP core has invalid size: %d", size);

	return 0;
}

static struct ip_type ip = {
	.vlnv = { "xilinx.com", "ip", "axi_bram_ctrl", NULL },
	.parse = bram_parse
};

REGISTER_IP_TYPE(&ip)