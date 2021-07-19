/** JSON serializtion for edgeFlex project.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstring>

#include <villas/timing.h>
#include <villas/formats/json_edgeflex.hpp>

using namespace villas::node;

int JsonEdgeflexFormat::packSample(json_t **json_smp, const struct sample *smp)
{
	json_error_t err;
	json_t *json_value;
	json_t *json_created = nullptr;

	if (smp->flags & (int) SampleFlags::HAS_TS_ORIGIN)
		json_created = json_integer(time_to_double(&smp->ts.origin) * 1e3);


	struct signal *sig = (struct signal *) vlist_at_safe(smp->signals, 0);
	if (!sig)
		return -1;


	json_value = json_pack_ex(&err, 0, "{ s: f }",
		"value", smp->data[0].f
	);

	if (json_created)
		json_object_set_new(json_value, "created", json_created);

	if (json_value == nullptr)
		return -1;

	*json_smp = json_value;

	return 0;
}

static char n[] = "json.edgeflex";
static char d[] = "EdgeFlex JSON format";
static FormatPlugin<JsonEdgeflexFormat, n, d, (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA> p;
