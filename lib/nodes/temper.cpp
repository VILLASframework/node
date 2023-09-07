/* PCSensor / TEMPer node-type.
 *
 * The driver will work with some TEMPer usb devices from RDing (www.PCsensor.com).
 *
 * Based on pcsensor.c by Juan Carlos Perez
 * Based on Temper.c by Robert Kavaler
 *
 * SPDX-FileCopyrightText: 2011 Juan Carlos Perez <cray@isp-sl.com>
 * SPDX-FileCopyrightText: 2009 Robert Kavaler (relavak.com)
 * SPDX-License-Identifier: BSD-1-Clause
 *
 * All rights reserved.
 *
 * 2011/08/30 Thanks to EdorFaus: bugfix to support negative temperatures
 * 2017/08/30 Improved by K.Cima: changed libusb-0.1 -> libusb-1.0
 *			https://github.com/shakemid/pcsensor
 */

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/temper.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::utils;
using namespace villas::node;

static Logger logger;

static std::list<TEMPerDevice *> devices;

static struct libusb_context *context;

// Forward declartions
static NodeCompatType p;

TEMPerDevice::TEMPerDevice(struct libusb_device *dev)
    : usb::Device(dev), scale(1.0), offset(0.0), timeout(5000) {
  int ret = libusb_get_device_descriptor(dev, &desc);
  if (ret != LIBUSB_SUCCESS)
    throw RuntimeError("Could not get USB device descriptor: {}",
                       libusb_strerror((enum libusb_error)ret));
}

void TEMPerDevice::open(bool reset) {
  int ret;

  usb::detach(handle, 0x00);
  usb::detach(handle, 0x01);

  if (reset)
    libusb_reset_device(handle);

  ret = libusb_set_configuration(handle, 0x01);
  if (ret < 0)
    throw RuntimeError("Could not set configuration 1");

  ret = libusb_claim_interface(handle, 0x00);
  if (ret < 0)
    throw RuntimeError("Could not claim interface. Error: {}", ret);

  ret = libusb_claim_interface(handle, 0x01);
  if (ret < 0)
    throw RuntimeError("Could not claim interface. Error: {}", ret);
}

void TEMPerDevice::close() {
  libusb_release_interface(handle, 0x00);
  libusb_release_interface(handle, 0x01);

  libusb_close(handle);
}

void TEMPerDevice::read(struct Sample *smp) {
  unsigned char answer[8];
  float temp[2];
  int i = 0, al, ret;

  // Read from device
  unsigned char question[sizeof(question_temperature)];
  memcpy(question, question_temperature, sizeof(question_temperature));

  ret = libusb_control_transfer(handle, 0x21, 0x09, 0x0200, 0x01, question, 8,
                                timeout);
  if (ret < 0)
    throw SystemError("USB control write failed: {}", ret);

  memset(answer, 0, 8);

  ret = libusb_interrupt_transfer(handle, 0x82, answer, 8, &al, timeout);
  if (al != 8)
    throw SystemError("USB interrupt read failed: {}", ret);

  decode(answer, temp);

  // Temperature 1
  smp->data[i++].f = temp[0];

  // Temperature 2
  if (getNumSensors() == 2)
    smp->data[i++].f = temp[1];

  // Humidity
  if (hasHumiditySensor())
    smp->data[i++].f = temp[1];

  smp->flags = 0;
  smp->length = i;
  smp->flags |= (int)SampleFlags::HAS_DATA;
}

// Thanks to https://github.com/edorfaus/TEMPered
void TEMPer1Device::decode(unsigned char *answer, float *temp) {
  int buf;

  // Temperature Celsius internal
  buf = ((signed char)answer[2] << 8) + (answer[3] & 0xFF);
  temp[0] = buf * (125.0 / 32000.0);
  temp[0] *= scale;
  temp[0] += offset;
}

void TEMPer2Device::decode(unsigned char *answer, float *temp) {
  int buf;

  TEMPer1Device::decode(answer, temp);

  // Temperature Celsius external
  buf = ((signed char)answer[4] << 8) + (answer[5] & 0xFF);
  temp[1] = buf * (125.0 / 32000.0);
  temp[1] *= scale;
  temp[1] += offset;
}

void TEMPerHUMDevice::decode(unsigned char *answer, float *temp) {
  int buf;

  // Temperature Celsius
  buf = ((signed char)answer[2] << 8) + (answer[3] & 0xFF);
  temp[0] = -39.7 + 0.01 * buf;
  temp[0] *= scale;
  temp[0] += offset;

  // Relative Humidity
  buf = ((signed char)answer[4] << 8) + (answer[5] & 0xFF);
  temp[1] = -2.0468 + 0.0367 * buf - 1.5955e-6 * buf * buf;
  temp[1] = (temp[0] - 25) * (0.01 + 0.00008 * buf) + temp[1];

  if (temp[1] < 0)
    temp[1] = 0;

  if (temp[1] > 99)
    temp[1] = 100;
}

TEMPerDevice *TEMPerDevice::make(struct libusb_device *dev) {
  if (TEMPerHUMDevice::match(dev))
    return new TEMPerHUMDevice(dev);
  else if (TEMPer2Device::match(dev))
    return new TEMPer2Device(dev);
  else if (TEMPer1Device::match(dev))
    return new TEMPer1Device(dev);
  else
    return nullptr;
}

bool TEMPer1Device::match(struct libusb_device *dev) {
  struct libusb_device_descriptor desc;
  int ret = libusb_get_device_descriptor(dev, &desc);
  if (ret < 0) {
    logging.get("node:temper")
        ->warn("Could not get USB device descriptor: {}",
               libusb_strerror((enum libusb_error)ret));
    return false;
  }

  return desc.idProduct == 0x7401 && desc.idVendor == 0x0c45;
}

bool TEMPer2Device::match(struct libusb_device *dev) {
  int ret;
  struct libusb_device_handle *handle;
  unsigned char product[256];

  struct libusb_device_descriptor desc;
  ret = libusb_get_device_descriptor(dev, &desc);
  if (ret < 0) {
    logging.get("node:temper")
        ->warn("Could not get USB device descriptor: {}",
               libusb_strerror((enum libusb_error)ret));
    return false;
  }

  ret = libusb_open(dev, &handle);
  if (ret < 0) {
    logging.get("node:temper")
        ->warn("Failed to open USB device: {}",
               libusb_strerror((enum libusb_error)ret));
    return false;
  }

  ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, product,
                                           sizeof(product));
  if (ret < 0) {
    logging.get("node:temper")
        ->warn("Could not get USB string descriptor: {}",
               libusb_strerror((enum libusb_error)ret));
    return false;
  }

  libusb_close(handle);

  return TEMPer1Device::match(dev) && getName() == (char *)product;
}

bool TEMPerHUMDevice::match(struct libusb_device *dev) {
  struct libusb_device_descriptor desc;
  int ret = libusb_get_device_descriptor(dev, &desc);
  if (ret < 0) {
    logging.get("node:temper")
        ->warn("Could not get USB device descriptor: {}",
               libusb_strerror((enum libusb_error)ret));
    return false;
  }

  return desc.idProduct == 0x7401 && desc.idVendor == 0x7402;
}

int villas::node::temper_type_start(villas::node::SuperNode *sn) {
  context = usb::get_context();

  logger = logging.get("node:temper");

  // Enumerate temper devices
  devices.clear();
  struct libusb_device **devs;

  int cnt = libusb_get_device_list(context, &devs);
  for (int i = 0; i < cnt; i++) {
    auto *dev = TEMPerDevice::make(devs[i]);

    logger->debug(
        "Found Temper device at bus={03d}, port={03d}, vendor_id={04x}, "
        "product_id={04x}, manufacturer={}, product={}, serial={}",
        dev->getBus(), dev->getPort(), dev->getDescriptor().idVendor,
        dev->getDescriptor().idProduct, dev->getManufacturer(),
        dev->getProduct(), dev->getSerial());

    devices.push_back(dev);
  }

  libusb_free_device_list(devs, 1);

  return 0;
}

int villas::node::temper_type_stop() {
  usb::deinit_context(context);

  return 0;
}

int villas::node::temper_init(NodeCompat *n) {
  auto *t = n->getData<struct temper>();

  t->calibration.scale = 1.0;
  t->calibration.offset = 0.0;

  t->filter.bus = -1;
  t->filter.port = -1;
  t->filter.vendor_id = -1;
  t->filter.product_id = -1;

  t->device = nullptr;

  return 0;
}

int villas::node::temper_destroy(NodeCompat *n) {
  auto *t = n->getData<struct temper>();

  if (t->device)
    delete t->device;

  return 0;
}

int villas::node::temper_parse(NodeCompat *n, json_t *json) {
  int ret;
  auto *t = n->getData<struct temper>();

  json_error_t err;

  ret = json_unpack_ex(json, &err, 0, "{ s?: { s?: F, s?: F}, s?: i, s?: i }",
                       "calibration", "scale", &t->calibration.scale, "offset",
                       &t->calibration.offset,

                       "bus", &t->filter.bus, "port", &t->filter.port);
  if (ret)
    throw ConfigError(json, err, "node-config-node-temper");

  return 0;
}

char *villas::node::temper_print(NodeCompat *n) {
  auto *t = n->getData<struct temper>();

  return strf("product=%s, manufacturer=%s, serial=%s humidity=%s, "
              "temperature=%d, usb.vendor_id=%#x, usb.product_id=%#x, "
              "calibration.scale=%f, calibration.offset=%f",
              t->device->getProduct(), t->device->getManufacturer(),
              t->device->getSerial(),
              t->device->hasHumiditySensor() ? "yes" : "no",
              t->device->getNumSensors(), t->device->getDescriptor().idVendor,
              t->device->getDescriptor().idProduct, t->calibration.scale,
              t->calibration.offset);
}

int villas::node::temper_prepare(NodeCompat *n) {
  auto *t = n->getData<struct temper>();

  // Find matching USB device
  t->device = nullptr;
  for (auto *dev : devices) {
    if (dev->match(&t->filter)) {
      t->device = dev;
      break;
    }
  }

  if (t->device == nullptr)
    throw RuntimeError("No matching TEMPer USB device found!");

  // Create signal list
  assert(n->getInputSignals(false)->size() == 0);

  // Temperature 1
  auto sig1 = std::make_shared<Signal>(
      t->device->getNumSensors() == 2 ? "temp_int" : "temp", "°C",
      SignalType::FLOAT);
  n->in.signals->push_back(sig1);

  // Temperature 2
  if (t->device->getNumSensors() == 2) {
    auto sig2 = std::make_shared<Signal>(
        t->device->getNumSensors() == 2 ? "temp_int" : "temp", "°C",
        SignalType::FLOAT);
    n->in.signals->push_back(sig2);
  }

  // Humidity
  if (t->device->hasHumiditySensor()) {
    auto sig3 = std::make_shared<Signal>("humidity", "%", SignalType::FLOAT);
    n->in.signals->push_back(sig3);
  }

  return 0;
}

int villas::node::temper_start(NodeCompat *n) {
  auto *t = n->getData<struct temper>();

  t->device->open();

  return 0;
}

int villas::node::temper_stop(NodeCompat *n) {
  auto *t = n->getData<struct temper>();

  t->device->close();

  return 0;
}

int villas::node::temper_read(NodeCompat *n, struct Sample *const smps[],
                              unsigned cnt) {
  auto *t = n->getData<struct temper>();

  assert(cnt == 1);

  t->device->read(smps[0]);

  return 1;
}

__attribute__((constructor(110))) static void register_plugin() {
  p.name = "temper";
  p.description = "An temper for staring new node-type implementations";
  p.vectorize = 1;
  p.flags = (int)NodeFactory::Flags::PROVIDES_SIGNALS;
  p.size = sizeof(struct temper);
  p.type.start = temper_type_start;
  p.type.stop = temper_type_stop;
  p.init = temper_init;
  p.destroy = temper_destroy;
  p.prepare = temper_prepare;
  p.parse = temper_parse;
  p.print = temper_print;
  p.start = temper_start;
  p.stop = temper_stop;
  p.read = temper_read;

  static NodeCompatFactory ncp(&p);
}
