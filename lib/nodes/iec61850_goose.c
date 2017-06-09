/** Node type: IEC 61850-8-1 (GOOSE)
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

#include <string.h>

#include "plugin.h"
#include "nodes/iec61850_goose.h"
#include "utils.h"
#include "msg.h"

int iec61850_goose_reverse(struct node *n)
{
	struct iec61850_goose *i __attribute__((unused)) = n->_vd;

	return 0;
}

int iec61850_goose_parse(struct node *n, config_setting_t *cfg)
{
	struct iec61850_goose *i __attribute__((unused)) = n->_vd;

	return 0;
}

char * iec61850_goose_print(struct node *n)
{
	char *buf = NULL;
	struct iec61850_goose *i __attribute__((unused)) = n->_vd;

	return buf;
}

int iec61850_goose_start(struct node *n)
{
	struct iec61850_goose *i __attribute__((unused)) = n->_vd;

	return 0;
}

int iec61850_goose_stop(struct node *n)
{
	struct iec61850_goose *i __attribute__((unused)) = n->_vd;

	return 0;
}

int iec61850_goose_deinit()
{

	return 0;
}

int iec61850_goose_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct iec61850_goose *i __attribute__((unused)) = n->_vd;

	return 0;
}

int iec61850_goose_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct iec61850_goose *i __attribute__((unused)) = n->_vd;

	return 0;
}

static struct plugin p = {
	.name		= "iec61850-8-1",
	.description	= "IEC 61850-8-1 (GOOSE)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct iec61850_goose),
		.reverse	= iec61850_goose_reverse,
		.parse		= iec61850_goose_parse,
		.print		= iec61850_goose_print,
		.start		= iec61850_goose_start,
		.stop		= iec61850_goose_stop,
		.deinit		= iec61850_goose_deinit,
		.read		= iec61850_goose_read,
		.write		= iec61850_goose_write,
		.instances	= LIST_INIT()
	}
};

REGISTER_PLUGIN(&p)