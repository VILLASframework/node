/* TEMPer node-type.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/format.hpp>
#include <villas/log.hpp>
#include <villas/node/config.hpp>
#include <villas/timing.hpp>
#include <villas/usb.hpp>

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;

class TEMPerDevice : public villas::usb::Device {

protected:
  constexpr static unsigned char question_temperature[] = {
      0x01, 0x80, 0x33, 0x01, 0x00, 0x00, 0x00, 0x00};

  float scale;
  float offset;

  int timeout;

  villas::Logger logger;

  virtual void decode(unsigned char *answer, float *temp) = 0;

public:
  static TEMPerDevice *make(struct libusb_device *desc);

  TEMPerDevice(struct libusb_device *dev);
  virtual ~TEMPerDevice() {}

  void open(bool reset = true);
  void close();

  virtual int getNumSensors() const { return 1; }

  virtual bool hasHumiditySensor() const { return false; }

  void read(struct Sample *smp);
};

class TEMPer1Device : public TEMPerDevice {

protected:
  virtual void decode(unsigned char *answer, float *temp);

  using TEMPerDevice::TEMPerDevice;

public:
  static bool match(struct libusb_device *dev);

  static std::string getName() { return "TEMPer1"; }
};

class TEMPer2Device : public TEMPer1Device {

protected:
  virtual void decode(unsigned char *answer, float *temp);

  using TEMPer1Device::TEMPer1Device;

public:
  static bool match(struct libusb_device *dev);

  static std::string getName() { return "TEMPer2"; }

  virtual int getNumSensors() const { return 2; }
};

class TEMPerHUMDevice : public TEMPerDevice {

protected:
  virtual void decode(unsigned char *answer, float *temp);

  using TEMPerDevice::TEMPerDevice;

public:
  static bool match(struct libusb_device *dev);

  static std::string getName() { return "TEMPerHUM"; }

  virtual bool hasHumiditySensor() const { return true; }
};

struct temper {
  struct {
    double scale;
    double offset;
  } calibration;

  struct villas::usb::Filter filter;

  TEMPerDevice *device;
};

int temper_type_start(SuperNode *sn);

int temper_type_stop();

int temper_init(NodeCompat *n);

int temper_prepare(NodeCompat *n);

int temper_destroy(NodeCompat *n);

int temper_parse(NodeCompat *n, json_t *json);

char *temper_print(NodeCompat *n);

int temper_check();

int temper_prepare();

int temper_start(NodeCompat *n);

int temper_stop(NodeCompat *n);

int temper_pause(NodeCompat *n);

int temper_resume(NodeCompat *n);

int temper_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int temper_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int temper_reverse(NodeCompat *n);

int temper_poll_fds(NodeCompat *n, int fds[]);

int temper_netem_fds(NodeCompat *n, int fds[]);

} // namespace node
} // namespace villas
