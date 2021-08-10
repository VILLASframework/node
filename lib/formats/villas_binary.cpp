/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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
#include <arpa/inet.h>

#include <villas/formats/villas_binary.hpp>
#include <villas/formats/msg.hpp>
#include <villas/formats/msg_format.hpp>
#include <villas/exceptions.hpp>
#include <villas/sample.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;

int VillasBinaryFormat::sprint(char *buf, size_t len, size_t *wbytes, const struct Sample * const smps[], unsigned cnt)
{
	int ret;
	unsigned i = 0;
	char *ptr = buf;

	for (i = 0; i < cnt; i++) {
		struct Message *msg = (struct Message *) ptr;
		const struct Sample *smp = smps[i];

		if (ptr + MSG_LEN(smp->length) > buf + len)
			break;

		ret = msg_from_sample(msg, smp, smp->signals, source_index);
		if (ret)
			return ret;

		if (web) {
			/** @todo convert to little endian */
		}
		else
			msg_hton(msg);

		ptr += MSG_LEN(smp->length);
	}

	if (wbytes)
		*wbytes = ptr - buf;

	return i;
}

int VillasBinaryFormat::sscan(const char *buf, size_t len, size_t *rbytes, struct Sample * const smps[], unsigned cnt)
{
	int ret, values;
	unsigned i, j;
	const char *ptr = buf;
	uint8_t sid; // source_index

	if (len % 4 != 0)
		return -1; /* Packet size is invalid: Must be multiple of 4 bytes */

	for (i = 0, j = 0; i < cnt; i++) {
		struct Message *msg = (struct Message *) ptr;
		struct Sample *smp = smps[j];

		smp->signals = signals;

		/* Complete buffer has been parsed */
		if (ptr == buf + len)
			break;

		/* Check if header is still in buffer bounaries */
		if (ptr + sizeof(struct Message) > buf + len)
			return -2; /* Invalid msg received */

		values = web ? msg->length : ntohs(msg->length);

		/* Check if remainder of message is in buffer boundaries */
		if (ptr + MSG_LEN(values) > buf + len)
			return -3; /* Invalid msg receive */

		if (web) {
			/** @todo convert from little endian */
		}
		else
			msg_ntoh(msg);

		ret = msg_to_sample(msg, smp, signals, &sid);
		if (ret)
			return ret; /* Invalid msg received */

		if (validate_source_index && sid != source_index) {
			// source index mismatch: we skip this sample
		}
		else
			j++;

		ptr += MSG_LEN(smp->length);
	}

	if (rbytes)
		*rbytes = ptr - buf;

	return j;
}

void VillasBinaryFormat::parse(json_t *json)
{
	int ret;
	json_error_t err;
	int sid = -1;
	int vsi = -1;

	ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: b }",
		"source_index", &sid,
		"validate_source_index", &vsi
	);
	if (ret)
		throw ConfigError(json, err, "node-config-format-villas-binary", "Failed to parse format configuration");

	if (vsi >= 0)
		validate_source_index = vsi != 0;

	if (sid >= 0)
		source_index = sid;

	Format::parse(json);
}

static VillasBinaryFormatPlugin<false> p1;
static VillasBinaryFormatPlugin<true> p2;
