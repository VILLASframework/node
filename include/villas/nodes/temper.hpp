/** An temper get started with new implementations of new node-types
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

/**
 * @addtogroup temper BSD temper Node Type
 * @ingroup node
 * @{
 */

#pragma once

#include <villas/node/config.h>
#include <villas/node.h>
#include <villas/format.hpp>
#include <villas/timing.h>
#include <villas/usb.hpp>
#include <villas/log.hpp>

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

	void read(struct sample *smp);
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

/** @see node_vtable::type_start */
int temper_type_start(villas::node::SuperNode *sn);

/** @see node_type::type_stop */
int temper_type_stop();

/** @see node_type::init */
int temper_init(struct vnode *n);

/** @see node_type::destroy */
int temper_destroy(struct vnode *n);

/** @see node_type::parse */
int temper_parse(struct vnode *n, json_t *json);

/** @see node_type::print */
char * temper_print(struct vnode *n);

/** @see node_type::check */
int temper_check();

/** @see node_type::prepare */
int temper_prepare();

/** @see node_type::start */
int temper_start(struct vnode *n);

/** @see node_type::stop */
int temper_stop(struct vnode *n);

/** @see node_type::pause */
int temper_pause(struct vnode *n);

/** @see node_type::resume */
int temper_resume(struct vnode *n);

/** @see node_type::write */
int temper_write(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @see node_type::read */
int temper_read(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @see node_type::reverse */
int temper_reverse(struct vnode *n);

/** @see node_type::poll_fds */
int temper_poll_fds(struct vnode *n, int fds[]);

/** @see node_type::netem_fds */
int temper_netem_fds(struct vnode *n, int fds[]);

/** @} */
