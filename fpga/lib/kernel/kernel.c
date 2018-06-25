/** Linux kernel related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <villas/utils.h>
#include <villas/config.h>
#include <villas/kernel/kernel.h>

int kernel_get_cacheline_size()
{
#ifdef __linux__
	return sysconf(_SC_LEVEL1_ICACHE_LINESIZE);
#else
	return 64; /** @todo fixme */
#endif
}

#ifdef __linux__

int kernel_module_set_param(const char *module, const char *param, const char *value)
{
	FILE *f;
	char fn[256];

	snprintf(fn, sizeof(fn), "%s/module/%s/parameters/%s", SYSFS_PATH, module, param);
	f = fopen(fn, "w");
	if (!f)
		serror("Failed set parameter %s for kernel module %s to %s", module, param, value);

	debug(LOG_KERNEL | 5, "Set parameter %s of kernel module %s to %s", module, param, value);
	fprintf(f, "%s", value);
	fclose(f);

	return 0;
}

int kernel_module_load(const char *module)
{
	int ret;

	ret = kernel_module_loaded(module);
	if (!ret) {
		debug(LOG_KERNEL | 5, "Kernel module %s already loaded...", module);
		return 0;
	}

	pid_t pid = fork();
	switch (pid) {
		case -1: // error
			return -1;

		case 0:  // child
			execlp("modprobe", "modprobe", module, (char *) 0);
			exit(EXIT_FAILURE);   // exec never returns

		default:
			wait(&ret);

			return kernel_module_loaded(module);
	}
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

int kernel_get_version(struct version *v)
{
	struct utsname uts;

	if (uname(&uts) < 0)
		return -1;

	if (version_parse(uts.release, v))
		return -1;

	return 0;
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

int kernel_get_page_size()
{
	return sysconf(_SC_PAGESIZE);
}

/* There is no sysconf interface to get the hugepage size */
int kernel_get_hugepage_size()
{
	char *key, *value, *unit, *line = NULL;
	int sz = -1;
	size_t len = 0;
	FILE *f;

	f = fopen(PROCFS_PATH "/meminfo", "r");
	if (!f)
		return -1;

	while (getline(&line, &len, f) != -1) {
		key   = strtok(line, ": ");
		value = strtok(NULL, " ");
		unit  = strtok(NULL, "\n");

		if (!strcmp(key, "Hugepagesize") && !strcmp(unit, "kB")) {
			sz = strtoul(value, NULL, 10) * 1024;
			break;
		}
	}

	free(line);
	fclose(f);

	return sz;
}

int kernel_get_nr_hugepages()
{
	FILE *f;
	int nr, ret;

	f = fopen(PROCFS_PATH "/sys/vm/nr_hugepages", "r");
	if (!f)
		serror("Failed to open %s", PROCFS_PATH "/sys/vm/nr_hugepages");

	ret = fscanf(f, "%d", &nr);
	if (ret != 1)
		nr = -1;

	fclose(f);

	return nr;
}

int kernel_set_nr_hugepages(int nr)
{
	FILE *f;

	f = fopen(PROCFS_PATH "/sys/vm/nr_hugepages", "w");
	if (!f)
		serror("Failed to open %s", PROCFS_PATH "/sys/vm/nr_hugepages");

	fprintf(f, "%d\n", nr);
	fclose(f);

	return 0;
}

#if 0
int kernel_has_cap(cap_value_t cap)
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

int kernel_irq_setaffinity(unsigned irq, uintmax_t affinity, uintmax_t *old)
{
	char fn[64];
	FILE *f;
	int ret = 0;

	snprintf(fn, sizeof(fn), "/proc/irq/%u/smp_affinity", irq);

	f = fopen(fn, "w+");
	if (!f)
		return errno;

	if (old)
		ret = fscanf(f, "%jx", old);

	fprintf(f, "%jx", affinity);
	fclose(f);

	return ret;
}

#endif /* __linux__ */
