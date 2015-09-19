/** Check system requirements.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include "utils.h"
#include "config.h"

int check_kernel_version()
{
	struct utsname uts;
	struct version current;
	struct version required = { KERNEL_VERSION_MAJ, KERNEL_VERSION_MIN };

	if (uname(&uts) < 0)
		return -1;

	if (version_parse(uts.release, &current))
		return -1;

	return version_compare(&current, &required) < 0;
}

int check_kernel_rtpreempt()
{
	return access(SYSFS_PATH "/kernel/realtime", R_OK);
}

int check_kernel_cmdline()
{
	char cmd[512];

	FILE *f = fopen(PROCFS_PATH "/cmdline", "r");
	if (!f)
		return -1;

	if (!fgets(cmd, sizeof(cmd), f))
		return -1;

	return strstr(cmd, "isolcpus=") ? 0 : -1;
}

int check_kernel_module(char *module)
{
	FILE *f;
	int ret = -1;
	char *line = NULL;
	size_t len = 0;

	if (!(f = fopen(PROCFS_PATH "/modules", "r")))
		return -1;

	while (getline(&line, &len, f) >= 0) {
		if (strstr(line, module) == line) {
			ret = 0;
			break;
		}
	}

	free(line);
	fclose(f);

	return ret;
}

int check_root()
{
	return getuid();
}

int check_license_time()
{
	return time(NULL) > LICENSE_VALID;
}

int check_license_trace()
{
	return ptrace(PTRACE_TRACEME, 0, 0, 0) < 0;
}

int check_license_ids()
{
	const struct {
		char *file, *content;
		off_t offset;
	} checks[] = LICENSE_CHECKS;

	FILE *f;
	char buf[128];
	
	for (int i = 0; i < ARRAY_LEN(checks); i++) {
		f = fopen(checks[i].file, "r");
		if (!f)
			return -1;

		fseek(f, checks[i].offset, SEEK_SET);
		fgets(buf, sizeof(buf), f);
		fclose(f);

		if (strncmp(buf, checks[i].content, strlen(checks[i].content)))
			return -1;
	}

	return 0;
}

