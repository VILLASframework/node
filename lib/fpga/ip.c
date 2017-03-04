/** FPGA IP component.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <libconfig.h>

#include "log.h"
#include "plugin.h"

int fpga_ip_init(struct fpga_ip *c)
{
	int ret;

	ret = c->_vt && c->_vt->init ? c->_vt->init(c) : 0;
	if (ret)
		error("Failed to intialize IP core: %s", c->name);

	if (ret == 0)
		c->state = IP_STATE_INITIALIZED;
	
	debug(8, "IP Core %s initalized (%u)", c->name, ret);

	return ret;
}

int fpga_ip_reset(struct fpga_ip *c)
{
	debug(3, "Reset IP core: %s", c->name);

	return c->_vt && c->_vt->reset ? c->_vt->reset(c) : 0;
}

int fpga_ip_destroy(struct fpga_ip *c)
{
	if (c->_vt && c->_vt->destroy)
		c->_vt->destroy(c);

	fpga_vlnv_destroy(&c->vlnv);
	
	return 0;
}

void fpga_ip_dump(struct fpga_ip *c)
{
	info("IP %s: vlnv=%s:%s:%s:%s baseaddr=%#jx, irq=%d, port=%d",
		c->name, c->vlnv.vendor, c->vlnv.library, c->vlnv.name, c->vlnv.version, 
		c->baseaddr, c->irq, c->port);

	if (c->_vt && c->_vt->dump)
		c->_vt->dump(c);
}

int fpga_ip_parse(struct fpga_ip *c, config_setting_t *cfg)
{
	int ret;
	const char *vlnv;
	long long baseaddr;

	c->cfg = cfg;

	c->name = config_setting_name(cfg);
	if (!c->name)
		cerror(cfg, "IP is missing a name");

	if (!config_setting_lookup_string(cfg, "vlnv", &vlnv))
		cerror(cfg, "IP %s is missing the VLNV identifier", c->name);

	ret = fpga_vlnv_parse(&c->vlnv, vlnv);
	if (ret)
		cerror(cfg, "Failed to parse VLNV identifier");

	/* Try to find matching IP type */
	list_foreach(struct plugin *l, &plugins) {
		if (l->type == PLUGIN_TYPE_FPGA_IP &&
		    !fpga_vlnv_cmp(&c->vlnv, &l->ip.vlnv)) {
			c->_vt = &l->ip;
			break;
		}
	}

	/* Common settings */
	if (config_setting_lookup_int64(cfg, "baseaddr", &baseaddr))
		c->baseaddr = baseaddr;
	else
		c->baseaddr = -1;

	if (!config_setting_lookup_int(cfg, "irq", &c->irq))
		c->irq = -1;
	if (!config_setting_lookup_int(cfg, "port", &c->port))
		c->port = -1;

	/* Type sepecific settings */
	ret = c->_vt && c->_vt->parse ? c->_vt->parse(c) : 0;
	if (ret)
		error("Failed to parse settings for IP core '%s'", c->name);

	return 0;
}
