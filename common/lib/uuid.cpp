/** UUID helpers.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#include <openssl/md5.h>

#include <villas/uuid.hpp>

using namespace villas::uuid;

int villas::uuid::generateFromString(uuid_t out, const std::string &data, const std::string &ns)
{
	int ret;
	MD5_CTX c;

	ret = MD5_Init(&c);
	if (!ret)
		return -1;

	/* Namespace */
	ret = MD5_Update(&c, (unsigned char *) ns.c_str(), ns.size());
	if (!ret)
		return -1;

	/* Data */
	ret = MD5_Update(&c, (unsigned char *) data.c_str(), data.size());
	if (!ret)
		return -1;

	ret = MD5_Final((unsigned char *) out, &c);
	if (!ret)
		return -1;

	return 0;
}

int villas::uuid::generateFromString(uuid_t out, const std::string &data, const uuid_t ns)
{
	int ret;
	MD5_CTX c;

	ret = MD5_Init(&c);
	if (!ret)
		return -1;

	/* Namespace */
	ret = MD5_Update(&c, (unsigned char *) ns, 16);
	if (!ret)
		return -1;

	/* Data */
	ret = MD5_Update(&c, (unsigned char *) data.c_str(), data.size());
	if (!ret)
		return -1;

	ret = MD5_Final((unsigned char *) out, &c);
	if (!ret)
		return -1;

	return 0;
}

int villas::uuid::generateFromJson(uuid_t out, json_t *json, const uuid_t ns)
{
	char *str = json_dumps(json, JSON_COMPACT | JSON_SORT_KEYS);

	int ret = generateFromString(out, str, ns);

	free(str);

	return ret;
}
