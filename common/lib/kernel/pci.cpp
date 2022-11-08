/** Linux PCI helpers
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#include <dirent.h>
#include <libgen.h>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/limits.h>

#include <villas/log.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/config.hpp>
#include <villas/kernel/pci.hpp>

using namespace villas::kernel::pci;

DeviceList::DeviceList()
{
	struct dirent *e;
	DIR *dp;
	FILE *f;
	char path[PATH_MAX];
	int ret;

	snprintf(path, sizeof(path), "%s/bus/pci/devices", SYSFS_PATH);

	dp = opendir(path);
	if (!dp)
		throw SystemError("Failed to detect PCI devices");

	while ((e = readdir(dp))) {
		/* Ignore special entries */
		if ((strcmp(e->d_name, ".") == 0) ||
		    (strcmp(e->d_name, "..") == 0) )
			continue;

		Id id;
		Slot slot;

		struct { const char *s; unsigned int *p; } map[] = {
			{ "vendor", &id.vendor },
			{ "device", &id.device }
		};

		/* Read vendor & device id */
		for (int i = 0; i < 2; i++) {
			snprintf(path, sizeof(path), "%s/bus/pci/devices/%s/%s", SYSFS_PATH, e->d_name, map[i].s);

			f = fopen(path, "r");
			if (!f)
				throw SystemError("Failed to open '{}'", path);

			ret = fscanf(f, "%x", map[i].p);
			if (ret != 1)
				throw RuntimeError("Failed to parse {} ID from: {}", map[i].s, path);

			fclose(f);
		}

		/* Get slot id */
		ret = sscanf(e->d_name, "%4x:%2x:%2x.%u", &slot.domain, &slot.bus, &slot.device, &slot.function);
		if (ret != 4)
			throw RuntimeError("Failed to parse PCI slot number: {}", e->d_name);

		emplace_back(std::make_shared<Device>(id, slot));
	}

	closedir(dp);
}

DeviceList::value_type
DeviceList::lookupDevice(const Slot &s)
{
	return *std::find_if(begin(), end(), [s](const DeviceList::value_type &d) {
		return d->slot == s;
	});
}

DeviceList::value_type
DeviceList::lookupDevice(const Id &i)
{
	return *std::find_if(begin(), end(), [i](const DeviceList::value_type &d) {
		return d->id == i;
	});
}

DeviceList::value_type
DeviceList::lookupDevice(const Device &d)
{
	auto dev = std::find_if(begin(), end(), [d](const DeviceList::value_type &e) {
		return *e == d;
	});

	return dev == end() ? value_type() : *dev;
}

Id::Id(const std::string &str) :
	vendor(0),
	device(0),
	class_code(0)
{
	char *s, *c, *e;
	char *tmp = strdup(str.c_str());

	if (!*tmp)
		return;

	s = strchr(tmp, ':');
	if (!s) {
		free(tmp);
			throw RuntimeError("Failed to parse PCI id: ':' expected", str);
	}

	*s++ = 0;
	if (tmp[0] && strcmp(tmp, "*")) {
		long int x = strtol(tmp, &e, 16);

		if ((e && *e) || (x < 0 || x > 0xffff)) {
			free(tmp);
			throw RuntimeError("Failed to parse PCI id: {}: Invalid vendor id", str);
		}

		vendor = x;
	}

	c = strchr(s, ':');
	if (c)
		*c++ = 0;

	if (s[0] && strcmp(s, "*")) {
		long int x = strtol(s, &e, 16);
		if ((e && *e) || (x < 0 || x > 0xffff)) {
			free(tmp);
			throw RuntimeError("Failed to parse PCI id: {}: Invalid device id", str);
		}

		device = x;
	}

	if (c && c[0] && strcmp(s, "*")) {
		long int x = strtol(c, &e, 16);

		if ((e && *e) || (x < 0 || x > 0xffff)) {
			free(tmp);
			throw RuntimeError("Failed to parse PCI id: {}: Invalid class code", str);
		}

		class_code = x;
	}
}

bool
Id::operator==(const Id &i)
{
	if ((i.device	!= 0 && i.device != device) ||
	    (i.vendor	!= 0 && i.vendor != vendor))
		return false;

	if ((i.class_code != 0) || (i.class_code != class_code))
		return false;

	return true;
}

Slot::Slot(const std::string &str) :
	domain(0),
	bus(0),
	device(0),
	function(0)
{
	char *tmp = strdup(str.c_str());
	char *colon = strrchr(tmp, ':');
	char *dot = strchr((colon ? colon + 1 : tmp), '.');
	char *mid = tmp;
	char *e, *buss, *colon2;

	if (colon) {
		*colon++ = 0;
		mid = colon;

		colon2 = strchr(tmp, ':');
		if (colon2) {
			*colon2++ = 0;
			buss = colon2;

			if (tmp[0] && strcmp(tmp, "*")) {
				long int x = strtol(tmp, &e, 16);
				if ((e && *e) || (x < 0 || x > 0x7fffffff)) {
					free(tmp);
					throw RuntimeError("Failed to parse PCI slot: {}: invalid domain", str);
				}

				domain = x;
			}
		}
		else
			buss = tmp;

		if (buss[0] && strcmp(buss, "*")) {
			long int x = strtol(buss, &e, 16);
			if ((e && *e) || (x < 0 || x > 0xff)) {
				free(tmp);
				throw RuntimeError("Failed to parse PCI slot: {}: invalid bus", str);
			}

			bus = x;
		}
	}

	if (dot)
		*dot++ = 0;

	if (mid[0] && strcmp(mid, "*")) {
		long int x = strtol(mid, &e, 16);

		if ((e && *e) || (x < 0 || x > 0x1f)) {
			free(tmp);
			throw RuntimeError("Failed to parse PCI slot: {}: invalid slot", str);
		}

		device = x;
	}

	if (dot && dot[0] && strcmp(dot, "*")) {
		long int x = strtol(dot, &e, 16);

		if ((e && *e) || (x < 0 || x > 7)) {
			free(tmp);
			throw RuntimeError("Failed to parse PCI slot: {}: invalid function", str);
		}

		function = x;
	}

	free(tmp);
}

bool
Slot::operator==(const Slot &s)
{
	if ((s.domain	!= 0 && s.domain   != domain) ||
	    (s.bus	!= 0 && s.bus      != bus) ||
	    (s.device	!= 0 && s.device   != device) ||
	    (s.function	!= 0 && s.function != function))
		return false;

	return true;
}

bool
Device::operator==(const Device &f)
{
	return id == f.id && slot == f.slot;
}

std::list<Region>
Device::getRegions() const
{
	FILE* f;
	char sysfs[1024];

	snprintf(sysfs, sizeof(sysfs), "%s/bus/pci/devices/%04x:%02x:%02x.%x/resource",
	         SYSFS_PATH, slot.domain, slot.bus, slot.device, slot.function);

	f = fopen(sysfs, "r");
	if (!f)
		throw SystemError("Failed to open resource mapping {}", sysfs);

	std::list<Region> regions;

	ssize_t bytesRead;
	char* line = nullptr;
	size_t len = 0;

	int reg_num = 0;

	/* Cap to 8 regions, just because we don't know how many may exist. */
	while (reg_num < 8 && (bytesRead = getline(&line, &len, f)) != -1) {
		unsigned long long tokens[3];
		char* s = line;
		for (int i = 0; i < 3; i++) {
			char* end;
			tokens[i] = strtoull(s, &end, 16);
			if (s == end) {
				printf("Error parsing line %d of %s\n", reg_num + 1, sysfs);
				tokens[0] = tokens[1] = 0; /* Mark invalid */
				break;
			}
			s = end;
		}

		free(line);

		/* Required for getline() to allocate a new buffer on the next iteration. */
		line = nullptr;
		len = 0;

		if (tokens[0] != tokens[1]) { /* This is a valid region */
			Region region;

			region.num = reg_num;
			region.start = tokens[0];
			region.end   = tokens[1];
			region.flags = tokens[2];

			regions.push_back(region);
		}

		reg_num++;
	}

	fclose(f);

	return regions;
}

std::string
Device::getDriver() const
{
	int ret;
	char sysfs[1024], syml[1024];
	memset(syml, 0, sizeof(syml));

	snprintf(sysfs, sizeof(sysfs), "%s/bus/pci/devices/%04x:%02x:%02x.%x/driver", SYSFS_PATH,
		slot.domain, slot.bus, slot.device, slot.function);

	struct stat st;
	ret = stat(sysfs, &st);
	if (ret)
		return "";

	ret = readlink(sysfs, syml, sizeof(syml));
	if (ret < 0)
		throw SystemError("Failed to follow link: {}", sysfs);

	return basename(syml);
}

bool
Device::attachDriver(const std::string &driver) const
{
	FILE *f;
	char fn[1024];

	/* Add new ID to driver */
	snprintf(fn, sizeof(fn), "%s/bus/pci/drivers/%s/new_id", SYSFS_PATH, driver.c_str());
	f = fopen(fn, "w");
	if (!f)
		throw SystemError("Failed to add PCI id to {} driver ({})", driver, fn);

	auto logger = logging.get("kernel:pci");
	logger->info("Adding ID to {} module: {:04x} {:04x}", driver, id.vendor, id.device);
	fprintf(f, "%04x %04x", id.vendor, id.device);
	fclose(f);

	/* Bind to driver */
	snprintf(fn, sizeof(fn), "%s/bus/pci/drivers/%s/bind", SYSFS_PATH, driver.c_str());
	f = fopen(fn, "w");
	if (!f)
		throw SystemError("Failed to bind PCI device to {} driver ({})", driver, fn);

	logger->info("Bind device to {} driver", driver);
	fprintf(f, "%04x:%02x:%02x.%x\n", slot.domain, slot.bus, slot.device, slot.function);
	fclose(f);

	return true;
}

int
Device::getIOMMUGroup() const
{
	int ret;
	char *group;
	//readlink does not add a null terminator!
	char link[1024] = {0};
	char sysfs[1024];

	snprintf(sysfs, sizeof(sysfs), "%s/bus/pci/devices/%04x:%02x:%02x.%x/iommu_group", SYSFS_PATH,
		slot.domain, slot.bus, slot.device, slot.function);

	ret = readlink(sysfs, link, sizeof(link));
	if (ret < 0)
		return -1;

	group = basename(link);

	return atoi(group);
}
