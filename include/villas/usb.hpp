/* Helpers for USB node-types.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libusb-1.0/libusb.h>

#include <villas/exceptions.hpp>

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
  Error(enum libusb_error e, const std::string &what)
      : std::runtime_error(what), err(e) {
    int ret __attribute__((unused));
    ret = asprintf(&msg, "%s: %s", std::runtime_error::what(),
                   libusb_strerror(err));
  }

  template <typename... Args>
  Error(enum libusb_error e, const std::string &what, Args &&...args)
      : usb::Error(e, fmt::format(what, std::forward<Args>(args)...)) {}

  // Same as above but with int
  Error(int e, const std::string &what)
      : usb::Error((enum libusb_error)e, what) {}

  template <typename... Args>
  Error(int e, const std::string &what, Args &&...args)
      : usb::Error((enum libusb_error)e, what, std::forward<Args>(args)...) {}

  ~Error() override {
    if (msg)
      free(msg);
  }

  const char *what() const noexcept override { return msg; }
};

class Device {

protected:
  struct libusb_device *device;
  struct libusb_device_handle *handle;
  struct libusb_device_descriptor desc;

  std::string getStringDescriptor(uint8_t desc_id) const;

public:
  Device(struct libusb_device *dev) : device(dev), handle(nullptr) {}

  const struct libusb_device_descriptor &getDescriptor() const { return desc; }

  int getBus() const { return libusb_get_bus_number(device); }

  int getPort() const { return libusb_get_port_number(device); }

  std::string getManufacturer() const {
    return getStringDescriptor(desc.iManufacturer);
  }

  std::string getProduct() const { return getStringDescriptor(desc.iProduct); }

  std::string getSerial() const {
    return getStringDescriptor(desc.iSerialNumber);
  }

  bool match(const Filter *flt) const;
};

struct libusb_context *init();

void deinit_context(struct libusb_context *ctx);

struct libusb_context *get_context();

void detach(struct libusb_device_handle *hdl, int iface);

} // namespace usb
} // namespace villas
