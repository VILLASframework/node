/** AXI Stream interconnect related helper functions
 *
 * These functions present a simpler interface to Xilinx' AXI Stream switch driver (XAxis_Switch_*)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include "list.h"
#include "log.h"
#include "plugin.h"

#include "fpga/switch.h"
#include "fpga/ip.h"

int switch_init(struct ip *c)
{
	int ret;
	struct sw *sw = &c->sw;
	struct fpga *f = c->card;

	XAxis_Switch *xsw = &sw->inst;

	if (c != f->sw)
		error("There can be only one AXI4-Stream interconnect per FPGA");


	/* Setup AXI-stream switch */
	XAxis_Switch_Config sw_cfg = {
		.BaseAddress = (uintptr_t) c->card->map + c->baseaddr,
		.MaxNumMI = sw->num_ports,
		.MaxNumSI = sw->num_ports
	};
	
	ret = XAxisScr_CfgInitialize(xsw, &sw_cfg, (uintptr_t) c->card->map + c->baseaddr);
	if (ret != XST_SUCCESS)
		return -1;

	/* Disable all masters */
	XAxisScr_RegUpdateDisable(xsw);
	XAxisScr_MiPortDisableAll(xsw);
	XAxisScr_RegUpdateEnable(xsw);

	return 0;
}

int switch_init_paths(struct ip *c)
{
	int ret;
	struct sw *sw = &c->sw;

	XAxis_Switch *xsw = &sw->inst;

	XAxisScr_RegUpdateDisable(xsw);
	XAxisScr_MiPortDisableAll(xsw);
	
	list_foreach(struct sw_path *p, &sw->paths) {
		struct ip *mi, *si;
		
		mi = list_lookup(&c->card->ips, p->out);
		si = list_lookup(&c->card->ips, p->in);
		
		if (!mi || !si || mi->port == -1 || si->port == -1)
			error("Invalid path configuration for FPGA");

		ret = switch_connect(c, mi, si);
		if (ret)
			error("Failed to configure switch");
	}

	XAxisScr_RegUpdateEnable(xsw);

	return 0;
}

void switch_destroy(struct ip *c)
{
	struct sw *sw = &c->sw;

	list_destroy(&sw->paths, NULL, true);
}

int switch_parse(struct ip *c)
{
	struct sw *sw = &c->sw;

	list_init(&sw->paths);

	config_setting_t *cfg_sw, *cfg_path;

	if (!config_setting_lookup_int(c->cfg, "num_ports", &sw->num_ports))
		cerror(c->cfg, "Switch IP '%s' requires 'num_ports' option", c->name);

	cfg_sw = config_setting_get_member(c->card->cfg, "paths");
	if (!cfg_sw)
		return 0; /* no switch config available */
	
	for (int i = 0; i < config_setting_length(cfg_sw); i++) {
		cfg_path = config_setting_get_elem(cfg_sw, i);

		struct sw_path path;
		int reverse;
		
		if (!config_setting_lookup_bool(cfg_path, "reverse", &reverse))
			reverse = 0;

		if (!config_setting_lookup_string(cfg_path, "in", &path.in) &&
		    !config_setting_lookup_string(cfg_path, "from", &path.in) &&
		    !config_setting_lookup_string(cfg_path, "src", &path.in) &&
		    !config_setting_lookup_string(cfg_path, "source", &path.in))
			cerror(cfg_path, "Path is missing 'in' setting");

		if (!config_setting_lookup_string(cfg_path, "out", &path.out) &&
		    !config_setting_lookup_string(cfg_path, "to", &path.out) &&
		    !config_setting_lookup_string(cfg_path, "dst", &path.out) &&
		    !config_setting_lookup_string(cfg_path, "dest", &path.out) &&
		    !config_setting_lookup_string(cfg_path, "sink", &path.out))
			cerror(cfg_path, "Path is missing 'out' setting");

		list_push(&sw->paths, memdup(&path, sizeof(path)));

		if (reverse) {
			const char *tmp = path.in;
			path.in = path.out;
			path.out = tmp;
			
			list_push(&sw->paths, memdup(&path, sizeof(path)));
		}
	}
	
	return 0;
}

int switch_connect(struct ip *c, struct ip *mi, struct ip *si)
{
	struct sw *sw = &c->sw;
	XAxis_Switch *xsw = &sw->inst;
	
	uint32_t mux, port;

	/* Check if theres already something connected */
	for (int i = 0; i < sw->num_ports; i++) {
		mux = XAxisScr_ReadReg(xsw->Config.BaseAddress, XAXIS_SCR_MI_MUX_START_OFFSET + i * 4);
		if (!(mux & XAXIS_SCR_MI_X_DISABLE_MASK)) {
			port = mux & ~XAXIS_SCR_MI_X_DISABLE_MASK;

			if (port == si->port) {
				warn("Switch: Slave port %s (%u) has been connected already to port %u. Disconnecting...", si->name, si->port, i);
				XAxisScr_RegUpdateDisable(xsw);
				XAxisScr_MiPortDisable(xsw, i);
				XAxisScr_RegUpdateEnable(xsw);
			}
		}
	}

	/* Reconfigure switch */
	XAxisScr_RegUpdateDisable(xsw);
	XAxisScr_MiPortEnable(xsw, mi->port, si->port);
	XAxisScr_RegUpdateEnable(xsw);
	
	/* Reset IPs */
	/*ip_reset(mi);
	ip_reset(si);*/
	
	debug(8, "FPGA: Switch connected %s (%u) to %s (%u)", mi->name, mi->port, si->name, si->port);

	return 0;
}

int switch_disconnect(struct ip *c, struct ip *mi, struct ip *si)
{
	struct sw *sw = &c->sw;
	XAxis_Switch *xsw = &sw->inst;

	if (!XAxisScr_IsMiPortEnabled(xsw, mi->port, si->port))
		return -1;

	XAxisScr_MiPortDisable(xsw, mi->port);

	return 0;
}

static struct plugin p = {
	.name		= "Xilinx's AXI4-Stream switch",
	.description	= "",
	.type		= PLUGIN_TYPE_FPGA_IP,
	.ip		= {
		.vlnv = { "xilinx.com", "ip", "axis_interconnect", NULL },
		.init = switch_init,
		.destroy = switch_destroy,
		.parse = switch_parse
	}
};

REGISTER_PLUGIN(&p)