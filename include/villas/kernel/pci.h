/** Linux PCI helpers
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#ifndef _PCI_H_
#define _PCI_H_

#include <pci/pci.h>

struct pci_access * pci_get_handle();

void pci_release_handle();

struct pci_dev * pci_find_device(struct pci_access *pacc, struct pci_filter *f);

int pci_attach_driver(struct pci_dev *d, const char *driver);

int pci_get_iommu_group(struct pci_dev *pdev);

#endif /* _PCI_H_ */