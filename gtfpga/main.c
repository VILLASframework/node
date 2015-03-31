#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pci/pci.h>

#define SYSFS_PATH "/sys/bus/pci"
#define GTFPGA_DID 0x10f5
#define GTFPGA_VID 0x8086

int main(int argc, char *argv[])
{
	struct pci_access *pacc;
	struct pci_dev *dev;
	struct pci_filter filter;

	unsigned int c;
	char namebuf[1024], *name;

	pacc = pci_alloc();		/* Get the pci_access structure */

	pci_init(pacc);			/* Initialize the PCI library */
	pci_scan_bus(pacc);		/* We want to get the list of devices */
	
	pci_filter_init(pacc, &filter);
	filter.vendor = GTFPGA_VID;
	filter.device = GTFPGA_DID;

	for (dev = pacc->devices; dev; dev = dev->next) {	/* Iterate over all devices */
		if (pci_filter_match(&filter, dev))
			break;
	}
	
	pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);	/* Fill in header info we need */

	c = pci_read_byte(dev, PCI_INTERRUPT_PIN);				/* Read config register directly */

	printf("%04x:%02x:%02x.%d vendor=%04x device=%04x class=%04x irq=%d (pin %d)",
		dev->domain, dev->bus, dev->dev, dev->func, dev->vendor_id, dev->device_id,
		dev->device_class, dev->irq, c);

	/* Look up and print the full name of the device */
	printf(" (%s)\n", pci_lookup_name(pacc, namebuf, sizeof(namebuf), PCI_LOOKUP_DEVICE, dev->vendor_id, dev->device_id));
	
	for (int i = 0; i< 6; i++) {
		printf("base_addr[%u] = %#lx, size = %#lx\n", i,
			dev->base_addr[i],
			dev->size[i]);
	}

	int fd = open("/dev/mem", O_RDWR);
	if (!fd)
		perror("Failed open(): ");
	
	void *map = mmap(NULL, dev->size[0], PROT_READ | PROT_WRITE, MAP_SHARED, fd, dev->base_addr[0]);
	
	if (map == MAP_FAILED)
		perror("Failed mmap(): ");

	for (int i = 0; i < 100; i++) {
		unsigned char *p = map + i;
		printf("%02hx ", *p);
		
		if (i % 32 == 31)
			printf("\n");
	}
	
	getchar();

	munmap(map, dev->size[0]);

	pci_cleanup(pacc);		/* Close everything */

	return 0;
}
