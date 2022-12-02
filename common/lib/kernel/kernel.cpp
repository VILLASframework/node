/** Linux kernel related functions.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#include <sys/utsname.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <cinttypes>

#include <sys/types.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <villas/log.hpp>
#include <villas/utils.hpp>
#include <villas/config.hpp>
#include <villas/kernel/kernel.hpp>

#include <villas/kernel/kernel.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::utils;

Version villas::kernel::getVersion()
{
	struct utsname uts;

	if (uname(&uts) < 0)
		throw SystemError("Failed to retrieve system identification");

	std::string rel = uts.release;

	// Remove release part. E.g. 4.9.93-linuxkit-aufs
	auto sep = rel.find('-');
	auto ver = rel.substr(0, sep - 1);

	return Version(ver);
}

int villas::kernel::getCachelineSize()
{
#if defined(__linux__) && defined(__x86_64__) && defined(__GLIBC__)
	return sysconf(_SC_LEVEL1_ICACHE_LINESIZE);
#elif defined(__MACH__)
	// Open the command for reading.
	FILE *fp = popen("sysctl -n machdep.cpu.cache.linesize", "r");
	if (fp == nullptr)
		return -1;

	int ret, size;

	ret = fscanf(fp, "%d", &size);

	pclose(fp);

	return ret == 1 ? size : -1;
#else
	return CACHELINE_SIZE;
#endif
}

#if defined(__linux__)
int villas::kernel::getPageSize()
{
	return sysconf(_SC_PAGESIZE);
}
#else
  #error "Unsupported platform"
#endif

// There is no sysconf interface to get the hugepage size
int villas::kernel::getHugePageSize()
{
#ifdef __linux__
	char *key, *value, *unit, *line = nullptr, *lasts;
	int sz = -1;
	size_t len = 0;
	FILE *f;

	f = fopen(PROCFS_PATH "/meminfo", "r");
	if (!f)
		return -1;

	while (getline(&line, &len, f) != -1) {
		key   = strtok_r(line, ": ", &lasts);
		value = strtok_r(nullptr, " ", &lasts);
		unit  = strtok_r(nullptr, "\n", &lasts);

		if (!strcmp(key, "Hugepagesize") && !strcmp(unit, "kB")) {
			sz = strtoul(value, nullptr, 10) * 1024;
			break;
		}
	}

	free(line);
	fclose(f);

	return sz;
#elif defined(__x86_64__)
	return 1 << 21;
#elif defined(__i386__)
	return 1 << 22;
#else
  #error "Unsupported architecture"
#endif
}

#ifdef __linux__

int villas::kernel::setModuleParam(const char *module, const char *param, const char *value)
{
	FILE *f;
	char fn[256];

	snprintf(fn, sizeof(fn), "%s/module/%s/parameters/%s", SYSFS_PATH, module, param);
	f = fopen(fn, "w");
	if (!f)
		throw RuntimeError("Failed set parameter {} for kernel module {} to {}", module, param, value);

	auto logger = logging.get("kernel");
	logger->debug("Set parameter {} of kernel module {} to {}", module, param, value);

	fprintf(f, "%s", value);
	fclose(f);

	return 0;
}

int villas::kernel::loadModule(const char *module)
{
	int ret;

	ret = isModuleLoaded(module);
	if (!ret) {
		auto logger = logging.get("kernel");
		logger->debug("Kernel module {} already loaded...", module);
		return 0;
	}

	pid_t pid = fork();
	switch (pid) {
		case -1: // Error
			return -1;

		case 0:  // Child
			execlp("modprobe", "modprobe", module, (char *) 0);
			exit(EXIT_FAILURE);   // exec() never returns

		default:
			wait(&ret);

			return isModuleLoaded(module);
	}
}

int villas::kernel::isModuleLoaded(const char *module)
{
	FILE *f;
	int ret = -1;
	char *line = nullptr;
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

int villas::kernel::getCmdlineParam(const char *param, char *buf, size_t len)
{
	int ret;
	char cmdline[512], key[128], value[128], *lasts, *tok;

	FILE *f = fopen(PROCFS_PATH "/cmdline", "r");
	if (!f)
		return -1;

	if (!fgets(cmdline, sizeof(cmdline), f))
		goto out;

	tok = strtok_r(cmdline, " \t", &lasts);
	do {
		ret = sscanf(tok, "%127[^=]=%127s", key, value);
		if (ret >= 1) {
			auto logger = logging.get("kernel");
			if (ret >= 2)
				logger->debug("Found kernel param: {}={}", key, value);
			else
				logger->debug("Found kernel param: {}", key);

			if (strcmp(param, key) == 0) {
				if (ret >= 2 && buf)
					snprintf(buf, len, "%s", value);

				return 0; // Found
			}
		}
	} while ((tok = strtok_r(nullptr, " \t", &lasts)));

out:
	fclose(f);

	return -1; // Not found or error
}

int villas::kernel::getNrHugepages()
{
	FILE *f;
	int nr, ret;

	f = fopen(PROCFS_PATH "/sys/vm/nr_hugepages", "r");
	if (!f) {
		auto logger = logging.get("kernel");
		logger->error("Failed to open {}: {}", PROCFS_PATH "/sys/vm/nr_hugepages", strerror(errno));
		return -1;
	}

	ret = fscanf(f, "%d", &nr);
	if (ret != 1)
		nr = -1;

	fclose(f);

	return nr;
}

int villas::kernel::setNrHugepages(int nr)
{
	FILE *f;

	f = fopen(PROCFS_PATH "/sys/vm/nr_hugepages", "w");
	if (!f)
		return -1;

	fprintf(f, "%d\n", nr);
	fclose(f);

	return 0;
}

int villas::kernel::setIRQAffinity(unsigned irq, uintmax_t aff, uintmax_t *old)
{
	char fn[64];
	FILE *f;
	int ret = 0;

	snprintf(fn, sizeof(fn), "/proc/irq/%u/smp_affinity", irq);

	f = fopen(fn, "w+");
	if (!f)
		return -1; // IRQ does not exist

	if (old)
		ret = fscanf(f, "%jx", old);

	fprintf(f, "%jx", aff);
	fclose(f);

	return ret;
}

int villas::kernel::get_cpu_frequency(uint64_t *freq)
{
	char *line = nullptr, *sep, *end;
	size_t len = 0;
	double dfreq;
	int ret;
	FILE *f;

	// Try to get CPU frequency from cpufreq module
	f = fopen(SYSFS_PATH "/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
	if (!f)
		goto cpuinfo;

	ret = fscanf(f, "%" PRIu64, freq);
	fclose(f);
	if (ret != 1)
		return -1;

	// cpufreq reports kHz
	*freq = *freq * 1000;

	return 0;

cpuinfo:
	// Try to read CPU frequency from /proc/cpuinfo
	f = fopen(PROCFS_PATH "/cpuinfo", "r");
	if (!f)
		return -1; // We give up here

	ret = -1;
	while (getline(&line, &len, f) >= 0) {
		if (strstr(line,  "cpu MHz") == line) {
			ret = 0;
			break;
		}
	}
	if (ret)
		goto out;

	sep = strchr(line, ':');
	if (!sep) {
		ret = -1;
		goto out;
	}

	dfreq = strtod(sep+1, &end);

	if (end == sep+1) {
		ret = -1;
		goto out;
	}

	// Frequency is given in MHz
	*freq = dfreq * 1e6;

out:	fclose(f);
	free(line);

	return ret;
}
#endif // __linux__
