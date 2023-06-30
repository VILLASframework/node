/** UUID helpers.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#include <openssl/evp.h>

#include <villas/uuid.hpp>

using namespace villas::uuid;

std::string villas::uuid::toString(const uuid_t in)
{
	uuid_string_t str;
	uuid_unparse_lower(in, str);
	return str;
}

int villas::uuid::generateFromString(uuid_t out, const std::string &data, const std::string &ns)
{
	int ret;
	EVP_MD_CTX *c = EVP_MD_CTX_new();

	ret = EVP_DigestInit(c, EVP_md5());
	if (!ret)
		return -1;

	// Namespace
	ret = EVP_DigestUpdate(c, (unsigned char *) ns.c_str(), ns.size());
	if (!ret)
		return -1;

	// Data
	ret = EVP_DigestUpdate(c, (unsigned char *) data.c_str(), data.size());
	if (!ret)
		return -1;

	ret = EVP_DigestFinal(c, (unsigned char *) out, nullptr);
	if (!ret)
		return -1;

	EVP_MD_CTX_free(c);

	return 0;
}

int villas::uuid::generateFromString(uuid_t out, const std::string &data, const uuid_t ns)
{
	int ret;
	EVP_MD_CTX *c = EVP_MD_CTX_new();

	ret = EVP_DigestInit(c, EVP_md5());
	if (!ret)
		return -1;

	// Namespace
	ret = EVP_DigestUpdate(c, (unsigned char *) ns, 16);
	if (!ret)
		return -1;

	// Data
	ret = EVP_DigestUpdate(c, (unsigned char *) data.c_str(), data.size());
	if (!ret)
		return -1;

	ret = EVP_DigestFinal(c, (unsigned char *) out, nullptr);
	if (!ret)
		return -1;

	EVP_MD_CTX_free(c);

	return 0;
}

int villas::uuid::generateFromJson(uuid_t out, json_t *json, const uuid_t ns)
{
	char *str = json_dumps(json, JSON_COMPACT | JSON_SORT_KEYS);

	int ret = generateFromString(out, str, ns);

	free(str);

	return ret;
}
