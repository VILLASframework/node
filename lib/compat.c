/** Compatability for different library versions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <string.h>
#include <jansson.h>

#include "compat.h"

#if JANSSON_VERSION_HEX < 0x020A00
size_t json_dumpb(const json_t *json, char *buffer, size_t size, size_t flags)
{
	char *str;
	size_t len;

	str = json_dumps(json, flags);
	if (!str)
		return 0;
	
	len = strlen(str); // not \0 terminated
	if (buffer && len <= size)
		memcpy(buffer, str, len);

	free(str);

	return len;
}
#endif