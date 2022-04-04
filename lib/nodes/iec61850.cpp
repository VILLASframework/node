/** Node type: IEC 61850-9-2 (Sampled Values)
 *
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

#include <cstring>
#include <pthread.h>
#include <unistd.h>

#include <map>
#include <array>

#include <villas/node_compat.hpp>
#include <villas/nodes/iec61850_sv.hpp>
#include <villas/signal.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>

#define CONFIG_SV_DEFAULT_APPID 0x4000
#define CONFIG_SV_DEFAULT_DST_ADDRESS CONFIG_GOOSE_DEFAULT_DST_ADDRESS
#define CONFIG_SV_DEFAULT_PRIORITY 4
#define CONFIG_SV_DEFAULT_VLAN_ID 0

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::node::iec61850;

const std::array<TypeDescriptor, 20> type_descriptors = {{
	/* name,              iec_type,                 type,                 size, supported */
	{ "boolean",          Type::BOOLEAN,		SignalType::BOOLEAN,	 1, false, false },
	{ "int8",             Type::INT8,		SignalType::INTEGER,	 1, false, false },
	{ "int16",            Type::INT16,		SignalType::INTEGER,	 2, false, false },
	{ "int32",            Type::INT32,		SignalType::INTEGER,	 4, false, false },
	{ "int64",            Type::INT64,		SignalType::INTEGER,	 8, false, false },
	{ "int8u",            Type::INT8U,		SignalType::INTEGER,	 1, false, false },
	{ "int16u",           Type::INT16U,		SignalType::INTEGER,	 2, false, false },
	{ "int32u",           Type::INT32U,		SignalType::INTEGER,	 4, false, false },
	{ "int64u",           Type::INT64U,		SignalType::INTEGER,	 8, false, false },
	{ "float32",          Type::FLOAT32,		SignalType::FLOAT,	 4, false, false },
	{ "float64",          Type::FLOAT64,		SignalType::FLOAT,	 8, false, false },
	{ "enumerated",       Type::ENUMERATED,		SignalType::INVALID,	 4, false, false },
	{ "coded_enum",       Type::CODED_ENUM,		SignalType::INVALID,	 4, false, false },
	{ "octet_string",     Type::OCTET_STRING,	SignalType::INVALID,	20, false, false },
	{ "visible_string",   Type::VISIBLE_STRING,	SignalType::INVALID,	35, false, false },
	{ "objectname",       Type::OBJECTNAME,		SignalType::INVALID,	20, false, false },
	{ "objectreference",  Type::OBJECTREFERENCE,	SignalType::INVALID,	20, false, false },
	{ "timestamp",        Type::TIMESTAMP,		SignalType::INVALID,	 8, false, false },
	{ "entrytime",        Type::ENTRYTIME,		SignalType::INVALID,	 6, false, false },
	{ "bitstring",        Type::BITSTRING,		SignalType::INVALID,	 4, false, false }
}};

/** Each network interface needs a separate receiver */
static std::map<std::string, BaseReceiver *> receivers;
static pthread_t thread;
static EthernetHandleSet hset;
static int users = 0;

static
void * iec61850_thread(void *ctx)
{
	while (true) {
		int ret = EthernetHandleSet_waitReady(hset, 1000);
		if (ret < 0)
			continue;

		for (auto it : receivers)
			it.second->tick();
	}

	return nullptr;
}

const TypeDescriptor * TypeDescriptor::lookup(const std::string &name)
{
	for (auto &td : type_descriptors) {
		if (name == td.name)
			return &td;
	}

	return nullptr;
}

const TypeDescriptor * TypeDescriptor::parse(json_t *json_signal, Signal::Ptr sig)
{
	int ret;
	const char *iec_type;

	ret = json_unpack(json_signal, "{ s?: s }",
		"iec_type", &iec_type
	);
	if (ret)
		return NULL;

	/* Try to deduct the IEC 61850 data type from VILLAS signal format */
	if (!iec_type) {
		if (!sig)
			iec_type = "float64";
		else {
			switch (sig->type) {
				case SignalType::BOOLEAN:
					iec_type = "boolean";
					break;

				case SignalType::FLOAT:
					iec_type = "float64";
					break;

				case SignalType::INTEGER:
					iec_type = "int64";
					break;

				default:
					return NULL;
			}
		}
	}

	return TypeDescriptor::lookup(iec_type);
}

int villas::node::iec61850::parseSignals(json_t *json_signals, std::vector<const TypeDescriptor*> &signals, SignalList::Ptr node_signals)
{
	int total_size = 0;

	signals.clear();

	json_t *json_signal;
	size_t i;
	json_array_foreach(json_signals, i, json_signal) {
		auto sig = node_signals ? node_signals->getByIndex(i) : Signal::Ptr();
		auto *td = TypeDescriptor::parse(json_signal, sig);
		if (!td)
			return -1;

		if (sig && td->type != sig->type)
			throw RuntimeError("Type mismatch for input signal '{}': type={}, iec_type={}", sig->name, signalTypeToString(sig->type), td->name);

		signals.push_back(td);
		total_size += td->size;
	}

	return total_size;
}

int villas::node::iec61850::type_start(villas::node::SuperNode *sn)
{
	int ret;

	/* Check if already initialized */
	if (users > 0)
		return 0;

	hset = EthernetHandleSet_new();

	ret = pthread_create(&thread, nullptr, iec61850_thread, nullptr);
	if (ret)
		return ret;

	return 0;
}

int villas::node::iec61850::type_stop()
{
	int ret;

	if (--users > 0)
		return 0;

	for (auto it : receivers)
		it.second->stop();

	ret = pthread_cancel(thread);
	if (ret)
		return ret;

	ret = pthread_join(thread, nullptr);
	if (ret)
		return ret;

	EthernetHandleSet_destroy(hset);

	return 0;
}

BaseReceiver::BaseReceiver(const std::string &intf)
{
	receivers[intf] = this;
}

void BaseReceiver::start()
{
	EthernetHandleSet_addSocket(hset, socket);
}

void BaseReceiver::stop()
{
	EthernetHandleSet_removeSocket(hset, socket);
}
