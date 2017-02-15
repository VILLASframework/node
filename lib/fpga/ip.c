#include <string.h>
#include <unistd.h>

#include <libconfig.h>

#include "log.h"

#include "fpga/ip.h"
#include "fpga/intc.h"
#include "fpga/fifo.h"
#include "nodes/fpga.h"

#include "config.h"

int ip_vlnv_cmp(struct ip_vlnv *a, struct ip_vlnv *b)
{
	return ((!a->vendor  || !b->vendor  || !strcmp(a->vendor,  b->vendor ))	&&
		(!a->library || !b->library || !strcmp(a->library, b->library))	&&
		(!a->name    || !b->name    || !strcmp(a->name,    b->name   ))	&&
		(!a->version || !b->version || !strcmp(a->version, b->version))) ? 0 : 1;
}

int ip_vlnv_parse(struct ip_vlnv *c, const char *vlnv)
{
	char *tmp = strdup(vlnv);

	c->vendor  = strdup(strtok(tmp, ":"));
	c->library = strdup(strtok(NULL, ":"));
	c->name    = strdup(strtok(NULL, ":"));
	c->version = strdup(strtok(NULL, ":"));
	
	free(tmp);

	return 0;
}

void ip_vlnv_destroy(struct ip_vlnv *v)
{
	free(c->vendor);
	free(c->library);
	free(c->name);
	free(c->version);
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

int ip_reset(struct ip *c)
{
	debug(3, "Reset IP core: %s", c->name);

	return c->_vt && c->_vt->reset ? c->_vt->reset(c) : 0;
}

void ip_destroy(struct ip *c)
{
	if (c->_vt && c->_vt->destroy)
		c->_vt->destroy(c);

	ip_vlnv_destroy(c->vlnv);
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

	ret = ip_vlnv_parse(&c->vlnv, vlnv);
	if (ret)
		cerror(cfg, "Failed to parse VLNV identifier");

	/* Try to find matching IP type */
	list_foreach(struct plugin *l, &plugins) {
		if (l->type == PLUGIN_TYPE_FPGA_IP &&
		    !ip_vlnv_match(&c->ip.vlnv, &l->ip.vlnv)) {
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
