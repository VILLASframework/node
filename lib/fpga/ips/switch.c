/** AXI Stream interconnect related helper functions
 *
 * These functions present a simpler interface to Xilinx' AXI Stream switch driver (XAxis_Switch_*)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 **********************************************************************************/

#include "list.h"
#include "log.h"
#include "log_config.h"
#include "plugin.h"

#include "fpga/ip.h"
#include "fpga/card.h"
#include "fpga/ips/switch.h"

int switch_start(struct fpga_ip *c)
{
	int ret;

	struct fpga_card *f = c->card;
	struct sw *sw = c->_vd;

	XAxis_Switch *xsw = &sw->inst;

	if (c != f->sw)
		error("There can be only one AXI4-Stream interconnect per FPGA");


	/* Setup AXI-stream switch */
	XAxis_Switch_Config sw_cfg = {
		.BaseAddress = (uintptr_t) f->map + c->baseaddr,
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

	switch_init_paths(c);

	return 0;
}

int switch_init_paths(struct fpga_ip *c)
{
	int ret;
	struct sw *sw = c->_vd;

	XAxis_Switch *xsw = &sw->inst;

	XAxisScr_RegUpdateDisable(xsw);
	XAxisScr_MiPortDisableAll(xsw);

	for (size_t i = 0; i < list_length(&sw->paths); i++) {
		struct sw_path *p = list_at(&sw->paths, i);
		struct fpga_ip *mi, *si;

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

int switch_destroy(struct fpga_ip *c)
{
	struct sw *sw = c->_vd;

	list_destroy(&sw->paths, NULL, true);

	return 0;
}

int switch_parse(struct fpga_ip *c)
{
	struct fpga_card *f = c->card;
	struct sw *sw = c->_vd;

	list_init(&sw->paths);

	config_setting_t *cfg_sw, *cfg_path;

	if (!config_setting_lookup_int(c->cfg, "num_ports", &sw->num_ports))
		cerror(c->cfg, "Switch IP '%s' requires 'num_ports' option", c->name);

	cfg_sw = config_setting_get_member(f->cfg, "paths");
	if (!cfg_sw)
		return 0; /* no switch config available */

	for (int i = 0; i < config_setting_length(cfg_sw); i++) {
		cfg_path = config_setting_get_elem(cfg_sw, i);

		struct sw_path *p = alloc(sizeof(struct sw_path));
		int reverse;

		if (!config_setting_lookup_bool(cfg_path, "reverse", &reverse))
			reverse = 0;

		if (!config_setting_lookup_string(cfg_path, "in", &p->in) &&
		    !config_setting_lookup_string(cfg_path, "from", &p->in) &&
		    !config_setting_lookup_string(cfg_path, "src", &p->in) &&
		    !config_setting_lookup_string(cfg_path, "source", &p->in))
			cerror(cfg_path, "Path is missing 'in' setting");

		if (!config_setting_lookup_string(cfg_path, "out", &p->out) &&
		    !config_setting_lookup_string(cfg_path, "to", &p->out) &&
		    !config_setting_lookup_string(cfg_path, "dst", &p->out) &&
		    !config_setting_lookup_string(cfg_path, "dest", &p->out) &&
		    !config_setting_lookup_string(cfg_path, "sink", &p->out))
			cerror(cfg_path, "Path is missing 'out' setting");

		list_push(&sw->paths, p);

		if (reverse) {
			struct sw_path *r = memdup(p, sizeof(struct sw_path));

			r->in = p->out;
			r->out = p->in;

			list_push(&sw->paths, r);
		}
	}

	return 0;
}

int switch_connect(struct fpga_ip *c, struct fpga_ip *mi, struct fpga_ip *si)
{
	struct sw *sw = c->_vd;
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

int switch_disconnect(struct fpga_ip *c, struct fpga_ip *mi, struct fpga_ip *si)
{
	struct sw *sw = c->_vd;
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
		.vlnv	= { "xilinx.com", "ip", "axis_interconnect", NULL },
		.type	= FPGA_IP_TYPE_MISC,
		.start	= switch_start,
		.destroy = switch_destroy,
		.parse	= switch_parse,
		.size	= sizeof(struct sw)
	}
};

REGISTER_PLUGIN(&p)
