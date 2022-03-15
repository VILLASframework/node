/** Helpers for USB node-types
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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

#pragma once

#include <villas/exceptions.hpp>

#include <libusb-1.0/libusb.h>

namespace villas {
namespace usb {

struct Filter {
	int bus;
	int port;
	int vendor_id;
	int product_id;
};

class Error : public std::runtime_error {

protected:
	enum libusb_error err;
	char *msg;

public:
	Error(enum libusb_error e, const std::string &what) :
		std::runtime_error(what),
		err(e)
	{
		int ret __attribute__((unused));
		ret = asprintf(&msg, "%s: %s", std::runtime_error::what(), libusb_strerror(err));
	}

	template<typename... Args>
	Error(enum libusb_error e, const std::string &what, Args&&... args) :
		usb::Error(e, fmt::format(what, std::forward<Args>(args)...))
	{ }

	/* Same as above but with int */
	Error(int e, const std::string &what) :
		usb::Error((enum libusb_error) e, what)
	{ }

	template<typename... Args>
	Error(int e, const std::string &what, Args&&... args) :
		usb::Error((enum libusb_error) e, what, std::forward<Args>(args)...)
	{ }

	~Error()
	{
		if (msg)
			free(msg);
	}

	virtual const char * what() const noexcept
	{
		return msg;
	}
};

class Device {

protected:
	struct libusb_device *device;
	struct libusb_device_handle *handle;
	struct libusb_device_descriptor desc;

	std::string getStringDescriptor(uint8_t desc_id) const;

public:
	Device(struct libusb_device *dev) :
		device(dev),
		handle(nullptr)
	{ }

	const struct libusb_device_descriptor & getDescriptor() const
	{ return desc; }

	int getBus() const
	{ return libusb_get_bus_number(device); }

	int getPort() const
	{ return libusb_get_port_number(device); }

	std::string getManufacturer() const
	{ return getStringDescriptor(desc.iManufacturer); }

	std::string getProduct() const
	{ return getStringDescriptor(desc.iProduct); }

	std::string getSerial() const
	{ return getStringDescriptor(desc.iSerialNumber); }

	bool match(const Filter *flt) const;
};

struct libusb_context * init();

void deinit_context(struct libusb_context *ctx);

struct libusb_context * get_context();

void detach(struct libusb_device_handle *hdl, int iface);

} /* namespace usb */
} /* namespace villas */
