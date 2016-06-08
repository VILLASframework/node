/** kernel related procedures.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utils.h"
#include "config.h"
#include "linux.h"

int kernel_module_set_param(const char *module, const char *param, const char *value)
{
	FILE *f;
	char fn[256];

	snprintf(fn, sizeof(fn), "%s/module/%s/parameters/%s", SYSFS_PATH, module, param);
	f = fopen(fn, "w");
	if (f) {
		debug(5, "Set parameter %s of kernel module %s to %s", module, param, value);
		fprintf(f, "%s", value);
		fclose(f);
	}
	else
		serror("Failed set parameter %s for kernel module %s to %s", module, param, value);

	return 0;
}

int kernel_module_load(const char *module)
{
	int ret;
	
	ret = kernel_module_loaded(module);
	if (!ret) {
		info("Kernel module %s already loaded...", module);
		return 0;
	}

	pid_t pid = fork();
	switch (pid) {
		case -1: // error
			return -1;

		case 0:  // child
			execlp("modprobe", "modprobe", module, (char *) 0);
			_exit(-1);   // exec never returns

		default:
			waitpid(pid, &ret, 0);
	}

	return ret;
}

int kernel_module_loaded(const char *module)
{
	FILE *f;
	int ret = -1;
	char *line = NULL;
	size_t len = 0;

	f = fopen(PROCFS_PATH "/modules", "r");
	if (!f)
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


int kernel_has_version(int maj, int min)
{
	struct utsname uts;
	struct version current;
	struct version required = { maj, min };

	if (uname(&uts) < 0)
		return -1;

	if (version_parse(uts.release, &current))
		return -1;

	return version_compare(&current, &required) < 0;
}

int kernel_is_rt()
{
	return access(SYSFS_PATH "/kernel/realtime", R_OK);
}

int kernel_has_cmdline(const char *substr)
{
	char cmd[512];

	FILE *f = fopen(PROCFS_PATH "/cmdline", "r");
	if (!f)
		return -1;

	if (!fgets(cmd, sizeof(cmd), f))
		return -1;

	return strstr(cmd, substr) ? 0 : -1;
}
int kernel_get_cacheline_size()
{
	return sysconf(_SC_LEVEL1_ICACHE_LINESIZE);
}