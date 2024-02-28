/** Compatability for different library versions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstring>
#include <jansson.h>
#include <unistd.h>

#include <villas/compat.h>

#if JANSSON_VERSION_HEX < 0x020A00
size_t json_dumpb(const json_t *json, char *buffer, size_t size, size_t flags)
{
	char *str;
	size_t len;

	str = json_dumps(json, flags);
	if (!str)
		return 0;

	len = strlen(str); /* not \0 terminated */
	if (buffer && len <= size)
		memcpy(buffer, str, len);

	free(str);

	return len;
}

static size_t json_loadfd_callback(void *buffer, size_t buflen, void *data)
{
	int *fd = (int *) data;

	return (size_t) read(*fd, buffer, buflen);
}

json_t *json_loadfd(int input, size_t flags, json_error_t *error)
{
	return json_load_callback(json_loadfd_callback, (void *) &input, flags, error);
}


static int json_dumpfd_callback(const char *buffer, size_t size, void *data)
{
#ifdef HAVE_UNISTD_H
	int *dest = (int *)data;

	if (write(*dest, buffer, size) == (ssize_t)size)
		return 0;
#else
	(void)buffer;
	(void)size;
	(void)data;
#endif /* HAVE_UNISTD_H */

	return -1;
}

int json_dumpfd(const json_t *json, int output, size_t flags)
{
    return json_dump_callback(json, json_dumpfd_callback, (void *) &output, flags);
}
#endif /* JANSSON_VERSION_HEX < 0x020A00 */
