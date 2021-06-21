/** PCSensor / TEMPer node-type
 *
 * Based on pcsensor.c by Juan Carlos Perez (c) 2011 (cray@isp-sl.com)
 * Based on Temper.c by Robert Kavaler (c) 2009 (relavak.com)
 *
 * All rights reserved.
 *
 * 2011/08/30 Thanks to EdorFaus: bugfix to support negative temperatures
 * 2017/08/30 Improved by K.Cima: changed libusb-0.1 -> libusb-1.0
 *			https://github.com/shakemid/pcsensor
 *
 * Temper driver for linux. This program can be compiled either as a library
 * or as a standalone program (-DUNIT_TEST). The driver will work with some
 * TEMPer usb devices from RDing (www.PCsensor.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY Juan Carlos Perez ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Robert kavaler BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <villas/node.h>
#include <villas/nodes/temper.hpp>
#include <villas/exceptions.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::utils;
using namespace villas::node;

static Logger logger;

static std::list<TEMPerDevice *> devices;

static struct libusb_context *context;

/* Forward declartions */
static struct vnode_type p;


TEMPerDevice::TEMPerDevice(struct libusb_device *dev) :
	usb::Device(dev),
	scale(1.0),
	offset(0.0),
	timeout(5000)
{
	int ret = libusb_get_device_descriptor(dev, &desc);
	if (ret != LIBUSB_SUCCESS)
		throw RuntimeError("Could not get USB device descriptor: {}", libusb_strerror((enum libusb_error) ret));
}

void TEMPerDevice::open(bool reset)
{
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

void TEMPerDevice::close()
{
	libusb_release_interface(handle, 0x00);
	libusb_release_interface(handle, 0x01);

	libusb_close(handle);
}

void TEMPerDevice::read(struct sample *smp)
{
	unsigned char answer[8];
	float temp[2];
	int i = 0, al, ret;

	/* Read from device */
	unsigned char question[sizeof(question_temperature)];
	memcpy(question, question_temperature, sizeof(question_temperature));

	ret = libusb_control_transfer(handle, 0x21, 0x09, 0x0200, 0x01, question, 8, timeout);
	if (ret < 0)
		throw SystemError("USB control write failed: {}", ret);

	memset(answer, 0, 8);

	ret = libusb_interrupt_transfer(handle, 0x82, answer, 8, &al, timeout);
	if (al != 8)
		throw SystemError("USB interrupt read failed: {}", ret);

	decode(answer, temp);

	/* Temperature 1 */
	smp->data[i++].f = temp[0];

	/* Temperature 2 */
	if (getNumSensors() == 2)
		smp->data[i++].f = temp[1];

	/* Humidity */
	if (hasHumiditySensor())
		smp->data[i++].f = temp[1];

	smp->flags = 0;
	smp->length = i;
	if (smp->length > 0)
		smp->flags |= (int) SampleFlags::HAS_DATA;
}

/* Thanks to https://github.com/edorfaus/TEMPered */
void TEMPer1Device::decode(unsigned char *answer, float *temp)
{
	int buf;

	// Temperature Celsius internal
	buf = ((signed char) answer[2] << 8) + (answer[3] & 0xFF);
	temp[0] = buf * (125.0 / 32000.0);
	temp[0] *= scale;
	temp[0] += offset;

}

void TEMPer2Device::decode(unsigned char *answer, float *temp)
{
	int buf;

	TEMPer1Device::decode(answer, temp);

	// Temperature Celsius external
	buf = ((signed char) answer[4] << 8) + (answer[5] & 0xFF);
	temp[1] = buf * (125.0 / 32000.0);
	temp[1] *= scale;
	temp[1] += offset;
}

void TEMPerHUMDevice::decode(unsigned char *answer, float *temp)
{
	int buf;

	// Temperature Celsius
	buf = ((signed char) answer[2] << 8) + (answer[3] & 0xFF);
	temp[0] = -39.7 + 0.01 * buf;
	temp[0] *= scale;
	temp[0] += offset;

	// Relative Humidity
	buf = ((signed char) answer[4] << 8) + (answer[5] & 0xFF);
	temp[1] = -2.0468 + 0.0367 * buf - 1.5955e-6 * buf * buf;
	temp[1] = (temp[0] - 25) * (0.01 + 0.00008 * buf) + temp[1];

	if (temp[1] <  0)
		temp[1] = 0;

	if (temp[1] > 99)
		temp[1] = 100;
}

TEMPerDevice * TEMPerDevice::make(struct libusb_device *dev)
{
	if (TEMPerHUMDevice::match(dev))
		return new TEMPerHUMDevice(dev);
	else if (TEMPer2Device::match(dev))
		return new TEMPer2Device(dev);
	else if (TEMPer1Device::match(dev))
		return new TEMPer1Device(dev);
	else
		return nullptr;
}

bool TEMPer1Device::match(struct libusb_device *dev)
{
	struct libusb_device_descriptor desc;
	int ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		logging.get("node:temper")->warn("Could not get USB device descriptor: {}", libusb_strerror((enum libusb_error) ret));
		return false;
	}

	return desc.idProduct == 0x7401 &&
	       desc.idVendor == 0x0c45;
}

bool TEMPer2Device::match(struct libusb_device *dev)
{
	int ret;
	struct libusb_device_handle *handle;
	unsigned char product[256];

	struct libusb_device_descriptor desc;
	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		logging.get("node:temper")->warn("Could not get USB device descriptor: {}", libusb_strerror((enum libusb_error) ret));
		return false;
	}

	ret = libusb_open(dev, &handle);
	if (ret < 0) {
		logging.get("node:temper")->warn("Failed to open USB device: {}", libusb_strerror((enum libusb_error) ret));
		return false;
	}

	ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, product, sizeof(product));
	if (ret < 0) {
		logging.get("node:temper")->warn("Could not get USB string descriptor: {}", libusb_strerror((enum libusb_error) ret));
		return false;
	}

	libusb_close(handle);

	return TEMPer1Device::match(dev) && getName() == (char *) product;
}

bool TEMPerHUMDevice::match(struct libusb_device *dev)
{
	struct libusb_device_descriptor desc;
	int ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		logging.get("node:temper")->warn("Could not get USB device descriptor: {}", libusb_strerror((enum libusb_error) ret));
		return false;
	}

	return desc.idProduct == 0x7401 &&
	       desc.idVendor  == 0x7402;
}

int temper_type_start(villas::node::SuperNode *sn)
{
	context = usb::get_context();

	logger = logging.get("node:temper");

	/* Enumerate temper devices */
	devices.clear();
	struct libusb_device **devs;

	int cnt = libusb_get_device_list(context, &devs);
	for (int i = 0; i < cnt; i++) {
		auto *dev = TEMPerDevice::make(devs[i]);

		logger->debug("Found Temper device at bus={03d}, port={03d}, vendor_id={04x}, product_id={04x}, manufacturer={}, product={}, serial={}",
			dev->getBus(), dev->getPort(),
			dev->getDescriptor().idVendor, dev->getDescriptor().idProduct,
			dev->getManufacturer(),
			dev->getProduct(),
			dev->getSerial());

		devices.push_back(dev);
	}

	libusb_free_device_list(devs, 1);

	return 0;
}

int temper_type_stop()
{
	usb::deinit_context(context);

	return 0;
}

int temper_init(struct vnode *n)
{
	struct temper *t = (struct temper *) n->_vd;

	t->calibration.scale = 1.0;
	t->calibration.offset = 0.0;

	t->filter.bus = -1;
	t->filter.port = -1;
	t->filter.vendor_id = -1;
	t->filter.product_id = -1;

	t->device = nullptr;

	return 0;
}

int temper_destroy(struct vnode *n)
{
	struct temper *t = (struct temper *) n->_vd;

	if (t->device)
		delete t->device;

	return 0;
}

int temper_parse(struct vnode *n, json_t *json)
{
	int ret;
	struct temper *t = (struct temper *) n->_vd;

	json_error_t err;

	ret = json_unpack_ex(json, &err, 0, "{ s?: { s?: F, s?: F}, s?: i, s?: i }",
		"calibration",
			"scale", &t->calibration.scale,
			"offset", &t->calibration.offset,

		"bus", &t->filter.bus,
		"port", &t->filter.port
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-temper");

	return 0;
}

char * temper_print(struct vnode *n)
{
	struct temper *t = (struct temper *) n->_vd;

	return strf("product=%s, manufacturer=%s, serial=%s humidity=%s, temperature=%d, usb.vendor_id=%#x, usb.product_id=%#x, calibration.scale=%f, calibration.offset=%f",
		t->device->getProduct(), t->device->getManufacturer(), t->device->getSerial(),
		t->device->hasHumiditySensor() ? "yes" : "no",
		t->device->getNumSensors(),
		t->device->getDescriptor().idVendor,
		t->device->getDescriptor().idProduct,
		t->calibration.scale,
		t->calibration.offset
	);
}

int temper_prepare(struct vnode *n)
{
	struct temper *t = (struct temper *) n->_vd;

	/* Find matching USB device */
	t->device = nullptr;
	for (auto *dev : devices) {
		if (dev->match(&t->filter)) {
			t->device = dev;
			break;
		}
	}

	if (t->device == nullptr)
		throw RuntimeError("No matching TEMPer USB device found!");

	/* Create signal list */
	assert(vlist_length(&n->in.signals) == 0);

	/* Temperature 1 */
	auto *sig1 = signal_create(t->device->getNumSensors() == 2 ? "temp_int" : "temp", "°C", SignalType::FLOAT);
	vlist_push(&n->in.signals, sig1);

	/* Temperature 2 */
	if (t->device->getNumSensors() == 2) {
		auto *sig2 = signal_create(t->device->getNumSensors() == 2 ? "temp_int" : "temp", "°C", SignalType::FLOAT);
		vlist_push(&n->in.signals, sig2);
	}

	/* Humidity */
	if (t->device->hasHumiditySensor()) {
		auto *sig3 = signal_create("humidity", "%", SignalType::FLOAT);
		vlist_push(&n->in.signals, sig3);
	}

	return 0;
}

int temper_start(struct vnode *n)
{
	struct temper *t = (struct temper *) n->_vd;

	t->device->open();

	return 0;
}

int temper_stop(struct vnode *n)
{
	struct temper *t = (struct temper *) n->_vd;

	t->device->close();

	return 0;
}

int temper_read(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	struct temper *t = (struct temper *) n->_vd;

	assert(cnt == 1);

	t->device->read(smps[0]);

	return 1;
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "temper";
	p.description	= "An temper for staring new node-type implementations";
	p.vectorize	= 1;
	p.flags		= (int) NodeFlags::PROVIDES_SIGNALS;
	p.size		= sizeof(struct temper);
	p.type.start	= temper_type_start;
	p.type.stop	= temper_type_stop;
	p.init		= temper_init;
	p.destroy	= temper_destroy;
	p.prepare	= temper_prepare;
	p.parse		= temper_parse;
	p.print		= temper_print;
	p.start		= temper_start;
	p.stop		= temper_stop;
	p.read		= temper_read;

	if (!node_types)
		node_types = new NodeTypeList();

	node_types->push_back(&p);
}
