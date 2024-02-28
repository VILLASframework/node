/** Linux PCI helpers
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <dirent.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>

#include <villas/log.h>
#include <villas/utils.h>
#include <villas/config.h>
#include <villas/kernel/pci.h>

int pci_init(struct pci *p)
{
	struct dirent *e;
	DIR *dp;
	FILE *f;
	char path[PATH_MAX];
	int ret;

	vlist_init(&p->devices);

	snprintf(path, sizeof(path), "%s/bus/pci/devices", SYSFS_PATH);

	dp = opendir(path);
	if (dp == NULL) {
		serror("Failed to detect PCI devices");
		return -1;
	}

	while ((e = readdir(dp))) {
		/* Ignore special entries */
		if ((strcmp(e->d_name, ".") == 0) ||
		    (strcmp(e->d_name, "..") == 0) )
			continue;

		struct pci_device *d = (struct pci_device *) alloc(sizeof(struct pci_device));

		struct { const char *s; int *p; } map[] = {
			{ "vendor", &d->id.vendor },
			{ "device", &d->id.device }
		};

		/* Read vendor & device id */
		for (int i = 0; i < 2; i++) {
			snprintf(path, sizeof(path), "%s/bus/pci/devices/%s/%s", SYSFS_PATH, e->d_name, map[i].s);

			f = fopen(path, "r");
			if (!f)
				serror("Failed to open '%s'", path);

			ret = fscanf(f, "%x", map[i].p);
			if (ret != 1)
				error("Failed to parse %s ID from: %s", map[i].s, path);

			fclose(f);
		}

		/* Get slot id */
		ret = sscanf(e->d_name, "%4x:%2x:%2x.%u", &d->slot.domain, &d->slot.bus, &d->slot.device, &d->slot.function);
		if (ret != 4)
			error("Failed to parse PCI slot number: %s", e->d_name);

		vlist_push(&p->devices, d);
	}

	closedir(dp);

	return 0;
}

int pci_destroy(struct pci *p)
{
	vlist_destroy(&p->devices, NULL, true);

	return 0;
}

int pci_device_parse_slot(struct pci_device *f, const char *s, const char **error)
{
	char *str = strdup(s);
	char *colon = strrchr(str, ':');
	char *dot = strchr((colon ? colon + 1 : str), '.');
	char *mid = str;
	char *e, *bus, *colon2;

	if (colon) {
		*colon++ = 0;
		mid = colon;

		colon2 = strchr(str, ':');
		if (colon2) {
			*colon2++ = 0;
			bus = colon2;

			if (str[0] && strcmp(str, "*")) {
				long int x = strtol(str, &e, 16);
				if ((e && *e) || (x < 0 || x > 0x7fffffff)) {
					*error = "Invalid domain number";
					goto fail;
				}

				f->slot.domain = x;
			}
		}
		else
			bus = str;

		if (bus[0] && strcmp(bus, "*")) {
			long int x = strtol(bus, &e, 16);
			if ((e && *e) || (x < 0 || x > 0xff)) {
				*error = "Invalid bus number";
				goto fail;
			}

			f->slot.bus = x;
		}
	}

	if (dot)
		*dot++ = 0;

	if (mid[0] && strcmp(mid, "*")) {
		long int x = strtol(mid, &e, 16);

		if ((e && *e) || (x < 0 || x > 0x1f)) {
			*error = "Invalid slot number";
			goto fail;
		}

		f->slot.device = x;
	}

	if (dot && dot[0] && strcmp(dot, "*")) {
		long int x = strtol(dot, &e, 16);

		if ((e && *e) || (x < 0 || x > 7)) {
			*error = "Invalid function number";
			goto fail;
		}

		f->slot.function = x;
	}

	free(str);
	return 0;

fail:
	free(str);
	return -1;
}

/* ID filter syntax: [vendor]:[device][:class] */
int pci_device_parse_id(struct pci_device *f, const char *str, const char **error)
{
	char *s, *c, *e;
	char *tmp = strdup(str);

	if (!*tmp)
		return 0;

	s = strchr(tmp, ':');
	if (!s) {
		*error = "':' expected";
		goto fail;
	}

	*s++ = 0;
	if (tmp[0] && strcmp(tmp, "*")) {
		long int x = strtol(tmp, &e, 16);

		if ((e && *e) || (x < 0 || x > 0xffff)) {
			*error = "Invalid vendor ID";
			goto fail;
		}

		f->id.vendor = x;
	}

	c = strchr(s, ':');
	if (c)
		*c++ = 0;

	if (s[0] && strcmp(s, "*")) {
		long int x = strtol(s, &e, 16);
		if ((e && *e) || (x < 0 || x > 0xffff)) {
			*error = "Invalid device ID";
			goto fail;
		}

		f->id.device = x;
	}

	if (c && c[0] && strcmp(s, "*")) {
		long int x = strtol(c, &e, 16);

		if ((e && *e) || (x < 0 || x > 0xffff)) {
			*error = "Invalid class code";
			goto fail;
		}

		f->id.class_code = x;
	}

	return 0;

fail:
	free(tmp);
	return -1;
}

int pci_device_compare(const struct pci_device *d, const struct pci_device *f)
{
	if ((f->slot.domain	!= 0 && f->slot.domain != d->slot.domain) ||
	    (f->slot.bus	!= 0 && f->slot.bus != d->slot.bus) ||
	    (f->slot.device	!= 0 && f->slot.device != d->slot.device) ||
	    (f->slot.function	!= 0 && f->slot.function != d->slot.function))
		return 1;

	if ((f->id.device	!= 0 && f->id.device != d->id.device) ||
	    (f->id.vendor	!= 0 && f->id.vendor != d->id.vendor))
		return 1;

	if ((f->id.class_code != 0) || (f->id.class_code != d->id.class_code))
		return 1;

	return 0; /* found */
}

struct pci_device * pci_lookup_device(struct pci *p, struct pci_device *f)
{
	return (struct pci_device *) vlist_search(&p->devices, (cmp_cb_t) pci_device_compare, (void *) f);
}

size_t pci_get_regions(const struct pci_device *d, struct pci_region** regions)
{
	FILE* f;
	char sysfs[1024];

	assert(regions != NULL);

	snprintf(sysfs, sizeof(sysfs), "%s/bus/pci/devices/%04x:%02x:%02x.%x/resource",
	         SYSFS_PATH, d->slot.domain, d->slot.bus, d->slot.device, d->slot.function);

	f = fopen(sysfs, "r");
	if (!f)
		serror("Failed to open resource mapping %s", sysfs);

	struct pci_region _regions[8];
	struct pci_region* cur_region = _regions;
	size_t valid_regions = 0;

	ssize_t bytesRead;
	char* line = NULL;
	size_t len = 0;

	int region = 0;

	/* Cap to 8 regions, just because we don't know how many may exist. */
	while (region < 8 && (bytesRead = getline(&line, &len, f)) != -1) {
		unsigned long long tokens[3];
		char* s = line;
		for (int i = 0; i < 3; i++) {
			char* end;
			tokens[i] = strtoull(s, &end, 16);
			if(s == end) {
				printf("Error parsing line %d of %s\n", region + 1, sysfs);
				tokens[0] = tokens[1] = 0; /* Mark invalid */
				break;
			}
			s = end;
		}

		free(line);

		/* Required for getline() to allocate a new buffer on the next iteration. */
		line = NULL;
		len = 0;

		if (tokens[0] != tokens[1]) {
			/* This is a valid region */
			cur_region->num = region;
			cur_region->start = tokens[0];
			cur_region->end = tokens[1];
			cur_region->flags = tokens[2];
			cur_region++;
			valid_regions++;
		}

		region++;
	}

	if (valid_regions > 0) {
		const size_t len = valid_regions * sizeof (struct pci_region);
		*regions = (struct pci_region *) malloc(len);
		memcpy(*regions, _regions, len);
	}

	return valid_regions;
}


int pci_get_driver(const struct pci_device *d, char *buf, size_t buflen)
{
	int ret;
	char sysfs[1024], syml[1024];
	memset(syml, 0, sizeof(syml));

	snprintf(sysfs, sizeof(sysfs), "%s/bus/pci/devices/%04x:%02x:%02x.%x/driver", SYSFS_PATH,
		d->slot.domain, d->slot.bus, d->slot.device, d->slot.function);

	ret = readlink(sysfs, syml, sizeof(syml));
	if (ret < 0)
		return ret;

	char *driver = basename(syml);

	strncpy(buf, driver, buflen);

	return 0;
}

int pci_attach_driver(const struct pci_device *d, const char *driver)
{
	FILE *f;
	char fn[1024];

	/* Add new ID to driver */
	snprintf(fn, sizeof(fn), "%s/bus/pci/drivers/%s/new_id", SYSFS_PATH, driver);
	f = fopen(fn, "w");
	if (!f)
		serror("Failed to add PCI id to %s driver (%s)", driver, fn);

	info("Adding ID to %s module: %04x %04x", driver, d->id.vendor, d->id.device);
	fprintf(f, "%04x %04x", d->id.vendor, d->id.device);
	fclose(f);

	/* Bind to driver */
	snprintf(fn, sizeof(fn), "%s/bus/pci/drivers/%s/bind", SYSFS_PATH, driver);
	f = fopen(fn, "w");
	if (!f)
		serror("Failed to bind PCI device to %s driver (%s)", driver, fn);

	info("Bind device to %s driver", driver);
	fprintf(f, "%04x:%02x:%02x.%x\n", d->slot.domain, d->slot.bus, d->slot.device, d->slot.function);
	fclose(f);

	return 0;
}

int pci_get_iommu_group(const struct pci_device *d)
{
	int ret;
	char *group, link[1024], sysfs[1024];

	snprintf(sysfs, sizeof(sysfs), "%s/bus/pci/devices/%04x:%02x:%02x.%x/iommu_group", SYSFS_PATH,
		d->slot.domain, d->slot.bus, d->slot.device, d->slot.function);

	ret = readlink(sysfs, link, sizeof(link));
	if (ret < 0)
		return -1;

	group = basename(link);

	return atoi(group);
}
