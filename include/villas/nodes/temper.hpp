/** An temper get started with new implementations of new node-types
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

#include <villas/node/config.hpp>
#include <villas/format.hpp>
#include <villas/timing.hpp>
#include <villas/usb.hpp>
#include <villas/log.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

class TEMPerDevice : public villas::usb::Device {

protected:
	constexpr static unsigned char question_temperature[] = { 0x01, 0x80, 0x33, 0x01, 0x00, 0x00, 0x00, 0x00 };

	float scale;
	float offset;

	int timeout;

	villas::Logger logger;

	virtual void decode(unsigned char *answer, float *temp) = 0;
public:
	static TEMPerDevice * make(struct libusb_device *desc);

	TEMPerDevice(struct libusb_device *dev);
	virtual ~TEMPerDevice()
	{ }

	void open(bool reset = true);
	void close();

	virtual int getNumSensors() const
	{ return 1; }

	virtual bool hasHumiditySensor() const
	{ return false; };

	void read(struct Sample *smp);
};

class TEMPer1Device : public TEMPerDevice {

protected:
	virtual void decode(unsigned char *answer, float *temp);

	using TEMPerDevice::TEMPerDevice;

public:
	static bool match(struct libusb_device *dev);

	static std::string getName()
	{ return "TEMPer1"; }
};

class TEMPer2Device : public TEMPer1Device {

protected:
	virtual void decode(unsigned char *answer, float *temp);

	using TEMPer1Device::TEMPer1Device;

public:
	static bool match(struct libusb_device *dev);

	static std::string getName()
	{ return "TEMPer2"; }

	virtual int getNumSensors() const
	{ return 2; }
};

class TEMPerHUMDevice : public TEMPerDevice {

protected:
	virtual void decode(unsigned char *answer, float *temp);

	using TEMPerDevice::TEMPerDevice;

public:
	static bool match(struct libusb_device *dev);

	static std::string getName()
	{ return "TEMPerHUM"; }

	virtual bool hasHumiditySensor() const
	{ return true; }
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

char * temper_print(NodeCompat *n);

int temper_check();

int temper_prepare();

int temper_start(NodeCompat *n);

int temper_stop(NodeCompat *n);

int temper_pause(NodeCompat *n);

int temper_resume(NodeCompat *n);

int temper_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int temper_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int temper_reverse(NodeCompat *n);

int temper_poll_fds(NodeCompat *n, int fds[]);

int temper_netem_fds(NodeCompat *n, int fds[]);

} /* namespace node */
} /* namespace villas */
