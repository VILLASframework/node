/** Node type: IEC 61850-9-2 (Sampled Values)
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

#include "nodes/iec61850_sv.h"
#include "plugin.h"

struct iec61850_type_descriptor type_descrptors[] = {
	[ IEC61850_TYPE_BOOLEAN ]         = { "boolean",          1, NULL,                    NULL,               NULL               },
	[ IEC61850_TYPE_INT8 ]            = { "int8",             1, SVClientASDU_getINT8,    SV_ASDU_addINT8,    SV_ASDU_setINT8    },
	[ IEC61850_TYPE_INT16 ]           = { "int16",            2, SVClientASDU_getINT16,   NULL,               NULL               },
	[ IEC61850_TYPE_INT32 ]           = { "int32",            4, SVClientASDU_getINT32,   SV_ASDU_addINT32,   SV_ASDU_setINT32   },
	[ IEC61850_TYPE_INT64 ]           = { "int64",            8, NULL,                    NULL,               NULL               },
	[ IEC61850_TYPE_INT8U ]           = { "int8u",            1, SVClientASDU_getINT8U,   NULL,               NULL               },
	[ IEC61850_TYPE_INT16U ]          = { "int16u",           2, NULL,                    NULL,               NULL               },
	[ IEC61850_TYPE_INT24U ]          = { "int24u",           3, NULL,                    NULL,               NULL               },
	[ IEC61850_TYPE_INT32U ]          = { "int32u",           4, SVClientASDU_getINT32U,  NULL,               NULL               },
	[ IEC61850_TYPE_FLOAT32 ]         = { "float32",          4, SVClientASDU_getFLOAT32, SV_ASDU_addFLOAT,   SV_ASDU_setFLOAT   },
	[ IEC61850_TYPE_FLOAT64 ]         = { "float64",          8, SVClientASDU_getFLOAT64, SV_ASDU_addFLOAT64, SV_ASDU_setFLOAT64 },
	[ IEC61850_TYPE_ENUMERATED ]      = { "enumerated",       4, NULL,                    NULL,               NULL               },
	[ IEC61850_TYPE_CODED_ENUM ]      = { "coded_enum",       4, NULL,                    NULL,               NULL               },
	[ IEC61850_TYPE_OCTET_STRING ]    = { "octect_string",   20, NULL,                    NULL,               NULL               },
	[ IEC61850_TYPE_VISIBLE_STRING ]  = { "visible_string",  35, NULL,                    NULL,               NULL               },
	[ IEC61850_TYPE_OBJECTNAME ]      = { "objectname"       20, NULL,                    NULL,               NULL               },
	[ IEC61850_TYPE_OBJECTREFERENCE ] = { "objectreference", 20, NULL,                    NULL,               NULL               },
	[ IEC61850_TYPE_TIMESTAMP ]       = { "timestamp",        8, NULL,                    NULL,               NULL               },
	[ IEC61850_TYPE_ENTRYTIME ]       = { "entrytime",        6, NULL,                    NULL,               NULL               },
	[ IEC61850_TYPE_BITSTRING ]       = { "bitstring",        4, NULL,                    NULL,               NULL               }
};

struct iec61850_type_descriptor * iec61850_type_lookup(const char *name)
{
	for (int i = 0; i < ARRAY_LEN(type_descriptors); i++) {
		if (!strcmp(name, type_descriptors[i].name))
			return &type_descriptors[i];
	}
	
	return NULL;
}

int iec61850_sv_reverse(struct node *n)
{
	struct iec61850_sv *i __attribute__((unused)) = n->_vd;

	return 0;
}

int iec61850_sv_parse(struct node *n, config_setting_t *cfg)
{
	struct iec61850_sv *i __attribute__((unused)) = n->_vd;
	
	config_setting_t *cfg_in, *cfg_out;

	cfg_in = config_setting_get_member(cfg, "in");
	if (cfg_in) {
		if ()
			cerror(cfg_out, "Failed to parse input for node %s", node_name(n));
	}
	
	cfg_out = config_setting_get_member(cfg, "out");
	if (cfg_out) {
		if ()
			cerror(cfg_out, "Failed to parse output for node %s", node_name(n));
	}

	return 0;
}

char * iec61850_sv_print(struct node *n)
{
	char *buf = NULL;
	struct iec61850_sv *i __attribute__((unused)) = n->_vd;

	return buf;
}

int iec61850_sv_start(struct node *n)
{
	struct iec61850_sv *i __attribute__((unused)) = n->_vd;
	
	SampledValuesPublisher svPublisher = SampledValuesPublisher_create(interface);

	SV_ASDU asdu1 = SampledValuesPublisher_addASDU(svPublisher, "svpub1", NULL, 1);
	       
	int float1 = SV_ASDU_addFLOAT(asdu1);
	int float2 = SV_ASDU_addFLOAT(asdu1);
	       
	SampledValuesPublisher_setupComplete(i->publisher);
	       
	return 0;
}

int iec61850_sv_stop(struct node *n)
{
	struct iec61850_sv *i = n->_vd;
	
        SampledValuesPublisher_destroy(i->publisher.handle);

	return 0;
}

int iec61850_sv_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct iec61850_sv *i __attribute__((unused)) = n->_vd;

	return 0;
}

int iec61850_sv_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct iec61850_sv *i = n->_vd;
	
	for (int i = 0; i < cnt; i++) {
	        SV_ASDU_setFLOAT(asdu1, float1, fVal1);
	}
	
	for (int j = 0; j < cnt; j++) {
		SV_ASDU *asdu = (SV_ASDU *) list_at(i->publisher.asdus, j);
		
		SV_ASDU_increaseSmpCnt(asdu);
	}
	
        SampledValuesPublisher_publish(i->publisher.handle);

	return 0;
}

static struct plugin p = {
	.name		= "iec61850-9-2",
	.description	= "IEC 61850-9-2 (Sampled Values)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct iec61850_sv),
		.reverse	= iec61850_sv_reverse,
		.parse		= iec61850_sv_parse,
		.print		= iec61850_sv_print,
		.start		= iec61850_sv_start,
		.stop		= iec61850_sv_stop,
		.read		= iec61850_sv_read,
		.write		= iec61850_sv_write,
		.instances	= LIST_INIT()
	}
};

REGISTER_PLUGIN(&p)