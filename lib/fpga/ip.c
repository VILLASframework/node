/** FPGA IP component.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include "log_config.h"
#include "log.h"
#include "plugin.h"

int fpga_ip_init(struct fpga_ip *c, struct fpga_ip_type *vt)
{
	int ret;

	assert(c->state == STATE_DESTROYED);

	c->_vt = vt;
	c->_vd = alloc(vt->size);

	ret = c->_vt->init ? c->_vt->init(c) : 0;
	if (ret)
		return ret;

	c->state = STATE_INITIALIZED;

	debug(8, "IP Core %s initalized (%u)", c->name, ret);

	return ret;
}

int fpga_ip_parse(struct fpga_ip *c, json_t *cfg, const char *name)
{
	int ret, baseaddr = -1;

	assert(c->state != STATE_STARTED && c->state != STATE_DESTROYED);

	c->name = strdup(name);
	c->baseaddr = -1;
	c->irq = -1;
	c->port = -1;

	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: i, s?: i }",
		"baseaddr", &baseaddr,
		"irq", &c->irq,
		"port", &c->port
	);
	if (ret)
		jerror(&err, "Failed to parse configuration for FPGA IP '%s'", name);

	c->baseaddr = baseaddr;

	/* Type sepecific settings */
	ret = c->_vt && c->_vt->parse ? c->_vt->parse(c, cfg) : 0;
	if (ret)
		error("Failed to parse settings for IP core '%s'", name);

	c->state = STATE_PARSED;

	return 0;
}

int fpga_ip_start(struct fpga_ip *c)
{
	int ret;

	assert(c->state == STATE_CHECKED);

	ret = c->_vt->start ? c->_vt->start(c) : 0;
	if (ret)
		return ret;

	c->state = STATE_STARTED;

	return 0;
}

int fpga_ip_stop(struct fpga_ip *c)
{
	int ret;

	assert(c->state == STATE_STARTED);

	ret = c->_vt->stop ? c->_vt->stop(c) : 0;
	if (ret)
		return ret;

	c->state = STATE_STOPPED;

	return 0;
}

int fpga_ip_destroy(struct fpga_ip *c)
{
	int ret;

	assert(c->state != STATE_DESTROYED);

	fpga_vlnv_destroy(&c->vlnv);

	ret = c->_vt->destroy ? c->_vt->destroy(c) : 0;
	if (ret)
		return ret;

	c->state = STATE_DESTROYED;

	free(c->_vd);

	return 0;
}

int fpga_ip_reset(struct fpga_ip *c)
{
	debug(3, "Reset IP core: %s", c->name);

	return c->_vt->reset ? c->_vt->reset(c) : 0;
}

void fpga_ip_dump(struct fpga_ip *c)
{
	assert(c->state != STATE_DESTROYED);

	info("IP %s: vlnv=%s:%s:%s:%s baseaddr=%#jx, irq=%d, port=%d",
		c->name, c->vlnv.vendor, c->vlnv.library, c->vlnv.name, c->vlnv.version,
		c->baseaddr, c->irq, c->port);

	if (c->_vt->dump)
		c->_vt->dump(c);
}

struct fpga_ip_type * fpga_ip_type_lookup(const char *vstr)
{
	int ret;

	struct fpga_vlnv vlnv;

	ret = fpga_vlnv_parse(&vlnv, vstr);
	if (ret)
		return NULL;

	/* Try to find matching IP type */
	for (size_t i = 0; i < list_length(&plugins); i++) {
		struct plugin *p = list_at(&plugins, i);

		if (p->type == PLUGIN_TYPE_FPGA_IP && !fpga_vlnv_cmp(&vlnv, &p->ip.vlnv))
			return &p->ip;
	}

	return NULL;
}
