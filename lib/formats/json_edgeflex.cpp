/** JSON serializtion for edgeFlex project.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/timing.hpp>
#include <villas/formats/json_edgeflex.hpp>

using namespace villas::node;

int JsonEdgeflexFormat::packSample(json_t **json_smp, const struct Sample *smp)
{
	json_t *json_data, *json_value;
	json_t *json_created = nullptr;

	if (smp->length < 1)
		return -1;

	json_data = json_object();

	for (unsigned i = 0; i < smp->length; i++) {
		auto sig = smp->signals->getByIndex(i);
		if (!sig)
			return -1;

		json_value = smp->data[i].toJson(sig->type);
		json_object_set(json_data, sig->name.c_str(), json_value);
	}

	json_created = json_integer(time_to_double(&smp->ts.origin) * 1e3);
	json_object_set(json_data, "created", json_created);

	*json_smp = json_data;

	return 0;
}

int JsonEdgeflexFormat::unpackSample(json_t *json_smp, struct Sample *smp)
{
	int ret;
	const char *key;
	json_t *json_value, *json_created = nullptr;
	json_int_t created = -1;

	if (smp->capacity < 1)
		return -1;

	if (json_typeof(json_smp) != JSON_OBJECT)
		return -1;

	json_object_foreach(json_smp, key, json_value) {
		if (!strcmp(key, "created"))
			json_created = json_incref(json_value);
		else {
			auto idx = signals->getIndexByName(key);
			if (idx < 0) {
				ret = sscanf(key, "signal_%d", &idx);
				if (ret != 1)
					continue;

				if (idx < 0)
					return -1;
			}

			auto sig = signals->getByIndex(idx);

			if (idx < (int) smp->capacity) {
				ret = smp->data[idx].parseJson(sig->type, json_value);
				if (ret)
					return ret;
			}

			if (idx >= (int) smp->length)
				smp->length = idx + 1;
		}
	}

	if (!json_created || !json_is_number(json_created))
		return -1;

	created = json_number_value(json_created);
	smp->ts.origin = time_from_double(created / 1e3);

	smp->flags = (int) SampleFlags::HAS_TS_ORIGIN;
	if (smp->length > 0)
		smp->flags |= (int) SampleFlags::HAS_DATA;

	return 0;
}

static char n[] = "json.edgeflex";
static char d[] = "EdgeFlex JSON format";
static FormatPlugin<JsonEdgeflexFormat, n, d, (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA> p;
