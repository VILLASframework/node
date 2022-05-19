/** UUID helpers.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
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
