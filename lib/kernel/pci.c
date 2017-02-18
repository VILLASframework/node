/** Linux PCI helpers
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Steffen Vogel
 **********************************************************************************/

#include <dirent.h>
#include <libgen.h>
#include <string.h>

#include "log.h"

#include "kernel/pci.h"
#include "config.h"

int pci_init(struct pci *p)
{
	struct dirent *entry;
	DIR *dp;
	FILE *f;
	char path[256];
	int ret;
	
	snprintf(path, sizeof(path), "%s/bus/pci/devices", SYSFS_PATH);

	dp = opendir(path);
	if (dp == NULL) {
		serror("Failed to detect PCI devices");
		return -1;
	}

	while ((entry = readdir(dp))) {
		struct pci_dev d;
		
		struct { const char *s; int *p; } map[] = {
			{ "vendor", &d.id.vendor },
			{ "device", &d.id.device }
		};
		
		/* Read vendor & device id */
		for (int i = 0; i < 2; i++) {
			snprintf(path, sizeof(path), "%s/bus/pci/devices/%s/%s", SYSFS_PATH, entry->d_name, map[i].s);
			
			f = fopen(path, "r");
			if (!f)
				serror("Failed to open '%s'", path);
			
			ret = fscanf(f, "%x", map[i].p);
			if (ret != 1)
				error("Failed to parse %s ID from: %s", map[i].s, path);

			fclose(f);
		}

		/* Get slot id */
		ret = sscanf(entry->d_name, "%4x:%2x:%2x.%u", &d.slot.domain, &d.slot.bus, &d.slot.device, &d.slot.function);
		if (ret != 4)
			error("Failed to parse PCI slot number: %s", entry->d_name);
		
		list_push(&p->devices, memdup(&d, sizeof(d)));
	}

	closedir(dp);
	
	return 0;
}

int pci_destroy(struct pci *p)
{
	list_destroy(&p->devices, NULL, true);
	
	return 0;
}

int pci_dev_parse_slot(struct pci_dev *f, const char *s, const char **error)
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
int pci_dev_parse_id(struct pci_dev *f, const char *str, const char **error)
{
	char *s, *c, *e;

	if (!*str)
		return 0;

	s = strchr(str, ':');
	if (!s) {
		*error = "':' expected";
		goto fail;
	}

	*s++ = 0;
	if (str[0] && strcmp(str, "*")) {
		long int x = strtol(str, &e, 16);
	
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

		f->id.class = x;
	}

	return 0;

fail:
	return -1;
}

int pci_dev_compare(const struct pci_dev *d, const struct pci_dev *f)
{
	if ((f->slot.domain >= 0 && f->slot.domain != d->slot.domain) ||
			(f->slot.bus >= 0 && f->slot.bus != d->slot.bus) ||
			(f->slot.device >= 0 && f->slot.device != d->slot.device) ||
			(f->slot.function >= 0 && f->slot.function != d->slot.function))
		return 0;

	if (f->id.device >= 0 || f->id.vendor >= 0) {
		if ((f->id.device >= 0 && f->id.device != d->id.device) || (f->id.vendor >= 0 && f->id.vendor != d->id.vendor))
			return 0;
	}
		
	if (f->id.class >= 0) {
		if (f->id.class != d->id.class)
			return 0;
	}

	return 1;
}

struct pci_dev * pci_lookup_device(struct pci *p, struct pci_dev *f)
{
	return list_search(&p->devices, (cmp_cb_t) pci_dev_compare, (void *) f);
}

int pci_attach_driver(struct pci_dev *d, const char *driver)
{
	FILE *f;
	char fn[256];

	/* Add new ID to driver */
	snprintf(fn, sizeof(fn), "%s/bus/pci/drivers/%s/new_id", SYSFS_PATH, driver);
	f = fopen(fn, "w");
	if (!f)
		serror("Failed to add PCI id to %s driver (%s)", driver, fn);

	debug(5, "Adding ID to %s module: %04x %04x", driver, d->id.vendor, d->id.device);
	fprintf(f, "%04x %04x", d->id.vendor, d->id.device);
	fclose(f);

	/* Bind to driver */
	snprintf(fn, sizeof(fn), "%s/bus/pci/drivers/%s/bind", SYSFS_PATH, driver);
	f = fopen(fn, "w");
	if (!f)
		serror("Failed to bind PCI device to %s driver (%s)", driver, fn);

	debug(5, "Bind device to %s driver", driver);
	fprintf(f, "%04x:%02x:%02x.%x\n", d->slot.domain, d->slot.bus, d->slot.device, d->slot.function);
	fclose(f);

	return 0;
}

int pci_get_iommu_group(struct pci_dev *d)
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
