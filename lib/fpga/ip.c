#include <string.h>
#include <unistd.h>

#include <libconfig.h>

#include "log.h"

#include "fpga/ip.h"
#include "fpga/intc.h"
#include "fpga/fifo.h"
#include "nodes/fpga.h"

#include "config-fpga.h"

struct list ip_types;	/**< Table of existing FPGA IP core drivers */

struct ip * ip_vlnv_lookup(struct list *l, const char *vendor, const char *library, const char *name, const char *version)
{
	list_foreach(struct ip *c, l) {
		if (ip_vlnv_match(c, vendor, library, name, version))
			return c;
	}

	return NULL;
}

int ip_vlnv_match(struct ip *c, const char *vendor, const char *library, const char *name, const char *version)
{
	return ((vendor  && strcmp(c->vlnv.vendor, vendor))	||
		(library && strcmp(c->vlnv.library, library))	||
		(name    && strcmp(c->vlnv.name, name))		||
		(version && strcmp(c->vlnv.version, version))) ? 0 : 1;
}

int ip_vlnv_parse(struct ip *c, const char *vlnv)
{
	char *tmp = strdup(vlnv);

	c->vlnv.vendor  = strdup(strtok(tmp, ":"));
	c->vlnv.library = strdup(strtok(NULL, ":"));
	c->vlnv.name    = strdup(strtok(NULL, ":"));
	c->vlnv.version = strdup(strtok(NULL, ":"));
	
	free(tmp);

	return 0;
}

int ip_init(struct ip *c)
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

void ip_destroy(struct ip *c)
{
	if (c->_vt && c->_vt->destroy)
		c->_vt->destroy(c);

	free(c->vlnv.vendor);
	free(c->vlnv.library);
	free(c->vlnv.name);
	free(c->vlnv.version);
}

void ip_dump(struct ip *c)
{
	info("IP %s: vlnv=%s:%s:%s:%s baseaddr=%#jx, irq=%d, port=%d",
		c->name, c->vlnv.vendor, c->vlnv.library, c->vlnv.name, c->vlnv.version, 
		c->baseaddr, c->irq, c->port);

	if (c->_vt && c->_vt->dump)
		c->_vt->dump(c);
}

int ip_parse(struct ip *c, config_setting_t *cfg)
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

	ret = ip_vlnv_parse(c, vlnv);
	if (ret)
		cerror(cfg, "Failed to parse VLNV identifier");

	/* Try to find matching IP type */
	list_foreach(struct ip_type *t, &ip_types) {
		if (ip_vlnv_match(c, t->vlnv.vendor, t->vlnv.library, t->vlnv.name, t->vlnv.version)) {
			c->_vt = t;
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
