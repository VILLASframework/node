/** Some helpers to libiec61850
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

#include <cstdint>

#include <netinet/ether.h>

#include <libiec61850/hal_ethernet.h>
#include <libiec61850/goose_receiver.h>
#include <libiec61850/sv_subscriber.h>

#include <villas/list.hpp>
#include <villas/signal.hpp>
#include <villas/signal_list.hpp>

#ifndef CONFIG_GOOSE_DEFAULT_DST_ADDRESS
  #define CONFIG_GOOSE_DEFAULT_DST_ADDRESS {0x01, 0x0c, 0xcd, 0x01, 0x00, 0x01}
#endif

namespace villas {
namespace node {
namespace iec61850 {

int type_start(villas::node::SuperNode *sn);
int type_stop();

enum class Type {
	/* According to IEC 61850-7-2 */
	BOOLEAN,
	INT8,
	INT16,
	INT32,
	INT64,
	INT8U,
	INT16U,
	INT32U,
	INT64U,
	FLOAT32,
	FLOAT64,
	ENUMERATED,
	CODED_ENUM,
	OCTET_STRING,
	VISIBLE_STRING,
	OBJECTNAME,
	OBJECTREFERENCE,
	TIMESTAMP,
	ENTRYTIME,

	/* According to IEC 61850-8-1 */
	BITSTRING
};

class TypeDescriptor {

public:
	std::string name;

	enum Type iec_type;
	enum SignalType type;

	unsigned size;

	bool publisher;
	bool subscriber;

	static
	const TypeDescriptor * lookup(const std::string &name);

	static
	const TypeDescriptor * parse(json_t *json_signal, Signal::Ptr sig);
};


int parseSignals(json_t *json_signals, std::vector<const TypeDescriptor *> &signals, SignalList::Ptr node_signals);

class BaseReceiver {

protected:
	std::string interface;
	EthernetSocket socket;

public:
	BaseReceiver(const std::string &intf);

	virtual
	void start();

	virtual
	void stop();

	virtual
	bool tick() = 0;
};

template<
	typename R,
	typename S,
	R (*_create)(),
	void (*_destroy)(R),
	bool (*_tick)(R),
	EthernetSocket (*_startThreadless)(R),
	void (*_stopThreadless)(R),
	void (*_addSubscriber)(R, S),
	void (*_removeSubscriber)(R, S),
	void (*_setInterfaceId)(R, const char*)
>
class Receiver : public BaseReceiver {

protected:
	R receiver;

public:
	Receiver(const std::string &intf) :
		BaseReceiver(intf),
		receiver(_create())
	{
		_setInterfaceId(receiver, interface.c_str());
	}

	virtual
	~Receiver()
	{
		_destroy(receiver);
	}

	virtual
	bool tick()
	{
		return _tick(receiver);
	}

	virtual
	void start()
	{
		socket = _startThreadless(receiver);
		Receiver::start();
	}

	virtual
	void stop()
	{
		Receiver::stop();
		_stopThreadless(receiver);
	}

	void addSubscriber(S sub)
	{
		_addSubscriber(receiver, sub);
	}

	void removeSubscriber(S sub)
	{
		_removeSubscriber(receiver, sub);
	}
};

using GooseReceiver = Receiver<
	::GooseReceiver,
	::GooseSubscriber,
	::GooseReceiver_create,
	::GooseReceiver_destroy,
	::GooseReceiver_tick,
	::GooseReceiver_startThreadless,
	::GooseReceiver_stopThreadless,
	::GooseReceiver_addSubscriber,
	::GooseReceiver_removeSubscriber,
	::GooseReceiver_setInterfaceId
>;

using SVReceiver = Receiver<
	::SVReceiver,
	::SVSubscriber,
	::SVReceiver_create,
	::SVReceiver_destroy,
	::SVReceiver_tick,
	::SVReceiver_startThreadless,
	::SVReceiver_stopThreadless,
	::SVReceiver_addSubscriber,
	::SVReceiver_removeSubscriber,
	::SVReceiver_setInterfaceId
>;

} /* namespace iec61850 */
} /* namespace node */
} /* namespace villas */
