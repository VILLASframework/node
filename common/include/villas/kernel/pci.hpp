/** Linux PCI helpers
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <list>

namespace villas {
namespace kernel {
namespace pci {

#define PCI_SLOT(devfn)		(((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)		((devfn) & 0x07)

class Id {
public:
	Id(const std::string &str);

	Id(int vid = 0, int did = 0, int cc = 0) :
		vendor(vid),
		device(did),
		class_code(cc)
	{ }

	bool
	operator==(const Id &i);

	unsigned int vendor;
	unsigned int device;
	unsigned int class_code;
};

class Slot {
public:
	Slot(const std::string &str);

	Slot(int dom = 0, int b = 0, int dev = 0, int fcn = 0) :
		domain(dom),
		bus(b),
		device(dev),
		function(fcn)
	{ }

	bool
	operator==(const Slot &s);

	unsigned int domain;
	unsigned int bus;
	unsigned int device;
	unsigned int function;
};

struct Region {
	int num;
	uintptr_t start;
	uintptr_t end;
	unsigned long long flags;
};

class Device {
public:
	Device(Id i, Slot s) :
		id(i),
		slot(s)
	{ }

	Device(Id i) :
		id(i)
	{ }

	Device(Slot s) :
		slot(s)
	{ }

	bool
	operator==(const Device &other);

	/** Get currently loaded driver for device */
	std::string
	getDriver() const;

	/** Bind a new LKM to the PCI device */
	bool
	attachDriver(const std::string &driver) const;

	/** Return the IOMMU group of this PCI device or -1 if the device is not in a group. */
	int
	getIOMMUGroup() const;

	std::list<Region>
	getRegions() const;

	Id id;
	Slot slot;
};

class DeviceList : public std::list<std::shared_ptr<Device>> {
public:
	/** Initialize Linux PCI handle.
	 *
	 * This search for all available PCI devices under /sys/bus/pci
	 */
	DeviceList();

	DeviceList::value_type
	lookupDevice(const Slot &s);

	DeviceList::value_type
	lookupDevice(const Id &i);

	DeviceList::value_type
	lookupDevice(const Device &f);
};

} // namespace pci
} // namespace kernel
} // namespace villas
