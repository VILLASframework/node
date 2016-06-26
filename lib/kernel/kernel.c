/** Linux kernel related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
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
#include "kernel/kernel.h"

int kernel_module_set_param(const char *module, const char *param, const char *value)
{
	FILE *f;
	char fn[256];

	snprintf(fn, sizeof(fn), "%s/module/%s/parameters/%s", SYSFS_PATH, module, param);
	f = fopen(fn, "w");
	if (!f)
		serror("Failed set parameter %s for kernel module %s to %s", module, param, value);
	
	debug(5, "Set parameter %s of kernel module %s to %s", module, param, value);
	fprintf(f, "%s", value);
	fclose(f);

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

int kernel_get_cmdline_param(const char *param, char *buf, size_t len)
{
	int ret;
	char cmdline[512];

	FILE *f = fopen(PROCFS_PATH "/cmdline", "r");
	if (!f)
		return -1;
	
	if (!fgets(cmdline, sizeof(cmdline), f))
		goto out;

	char *tok = strtok(cmdline, " \t");
	do {
		char key[128], value[128];

		ret = sscanf(tok, "%127[^=]=%127s", key, value);
		if (ret >= 1) {
			if (ret >= 2)
				debug(30, "Found kernel param: %s=%s", key, value);
			else
				debug(30, "Found kernel param: %s", key);

			if (strcmp(param, key) == 0) {
				if (ret >= 2 && buf)
					strncpy(buf, value, len);

				return 0; /* found */
			}
		}
	} while((tok = strtok(NULL, " \t")));

out:
	fclose(f);

	return -1; /* not found or error */
}

int kernel_get_cacheline_size()
{
	return sysconf(_SC_LEVEL1_ICACHE_LINESIZE);
}

#if 0
int kernel_check_cap(cap_value_t cap)
{
	int ret;

	cap_t caps;
	cap_flag_value_t value;
	
	caps = cap_get_proc();
	if (caps == NULL)
		return -1;

	ret = cap_get_proc(caps);
	if (ret == -1)
		return -1;
	
	ret = cap_get_flag(caps, cap, CAP_EFFECTIVE, &value);
	if (ret == -1)
		return -1;
	
	ret = cap_free(caps);	
	if (ret)
		return -1;
	
	return value == CAP_SET ? 0 : -1;
}
#endif

int kernel_irq_setaffinity(unsigned irq, uintmax_t new, uintmax_t *old)
{
	char fn[64];
	FILE *f;
	
	snprintf(fn, sizeof(fn), "/proc/irq/%u/smp_affinity", irq);
	
	f = fopen(fn, "w+");
	if (!f)
		return -1; /* IRQ does not exist */

	if (old)
		fscanf(f, "%jx", old);

	fprintf(f, "%jx", new);
	fclose(f);

	return 0;
}
