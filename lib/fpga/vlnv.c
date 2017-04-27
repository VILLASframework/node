/** Vendor, Library, Name, Version (VLNV) tag
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

#include <stdlib.h>
#include <string.h>

#include "fpga/vlnv.h"
#include "fpga/ip.h"

struct fpga_ip * fpga_vlnv_lookup(struct list *l, struct fpga_vlnv *v)
{
	return (struct fpga_ip *) list_search(l, (cmp_cb_t) fpga_vlnv_cmp, v);
}

int fpga_vlnv_cmp(struct fpga_vlnv *a, struct fpga_vlnv *b)
{
	return ((!a->vendor  || !b->vendor  || !strcmp(a->vendor,  b->vendor ))	&&
		(!a->library || !b->library || !strcmp(a->library, b->library))	&&
		(!a->name    || !b->name    || !strcmp(a->name,    b->name   ))	&&
		(!a->version || !b->version || !strcmp(a->version, b->version))) ? 0 : 1;
}

int fpga_vlnv_parse(struct fpga_vlnv *c, const char *vlnv)
{
	char *tmp = strdup(vlnv);

	c->vendor  = strdup(strtok(tmp, ":"));
	c->library = strdup(strtok(NULL, ":"));
	c->name    = strdup(strtok(NULL, ":"));
	c->version = strdup(strtok(NULL, ":"));
	
	free(tmp);

	return 0;
}

int fpga_vlnv_destroy(struct fpga_vlnv *v)
{
	free(v->vendor);
	free(v->library);
	free(v->name);
	free(v->version);
	
	return 0;
}
