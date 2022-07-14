/** Node type: IEC60870-5-104
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

#include <algorithm>
#include <villas/node_compat.hpp>
#include <villas/nodes/iec60870.hpp>
#include <villas/utils.hpp>
#include <villas/sample.hpp>
#include <villas/exceptions.hpp>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::node::iec60870;
using namespace std::literals::chrono_literals;

CP56Time2a timespec_to_cp56time2a(timespec time) {
	time_t time_ms =
		static_cast<time_t> (time.tv_sec) * 1000
		+ static_cast<time_t> (time.tv_nsec) / 1000000;
	return CP56Time2a_createFromMsTimestamp(NULL, time_ms);
}

timespec cp56time2a_to_timespec(CP56Time2a cp56time2a) {
	auto time_ms = CP56Time2a_toMsTimestamp(cp56time2a);
	timespec time {};
	time.tv_nsec = time_ms % 1000 * 1000;
	time.tv_sec = time_ms / 1000;
	return time;
}

ASDUData ASDUData::parse(json_t *signal_json, std::optional<ASDUData> last_data, bool duplicate_ioa_is_sequence) {
	json_error_t err;
	char const *asdu_type_name = nullptr;
	int with_timestamp = -1;
	char const *asdu_type_id = nullptr;
	std::optional<int> ioa_sequence_start = std::nullopt;
	int ioa = -1;

	if (json_unpack_ex(signal_json, &err, 0, "{ s?: s, s?: b, s?: s, s: i }",
		"asdu_type", &asdu_type_name,
		"with_timestamp", &with_timestamp,
		"asdu_type_id", &asdu_type_id,
		"ioa", &ioa
	))
		throw ConfigError(signal_json, err, "node-config-node-iec60870-5-104");

	with_timestamp = with_timestamp != -1 ? with_timestamp != 0 : false;

	// increase the ioa if it is found twice to make it a sequence
	if (	duplicate_ioa_is_sequence &&
		last_data &&
		ioa == last_data->ioa_sequence_start) {
		ioa = last_data->ioa + 1;
		ioa_sequence_start = last_data->ioa_sequence_start;
	}

	if (	(asdu_type_name && asdu_type_id) ||
		(!asdu_type_name && !asdu_type_id))
		throw RuntimeError("Please specify one of asdu_type or asdu_type_id", ioa);

	auto asdu_data = asdu_type_name
		? ASDUData::lookupName(asdu_type_name, with_timestamp, ioa, ioa_sequence_start.value_or(ioa))
		: ASDUData::lookupTypeId(asdu_type_id, ioa, ioa_sequence_start.value_or(ioa));

	if (!asdu_data.has_value())
		throw RuntimeError("Found invalid asdu_type or asdu_type_id");

	if (asdu_type_id && with_timestamp != -1 && asdu_data->hasTimestamp() != with_timestamp)
		throw RuntimeError("Found mismatch between asdu_type_id {} and with_timestamp {}", asdu_type_id, with_timestamp != 0);

	return *asdu_data;
};

std::optional<ASDUData> ASDUData::lookupTypeId(char const *type_id, int ioa, int ioa_sequence_start)
{
	auto check = [type_id] (Descriptor descriptor) {
		return !strcmp(descriptor.type_id, type_id);
	};
	auto descriptor = std::find_if(begin(descriptors), end(descriptors), check);
	if (descriptor != end(descriptors))
		return ASDUData { &*descriptor, ioa, ioa_sequence_start };
	else
		return std::nullopt;
}

std::optional<ASDUData> ASDUData::lookupName(char const *name, bool with_timestamp, int ioa, int ioa_sequence_start)
{
	auto check = [name, with_timestamp] (Descriptor descriptor) {
		return !strcmp(descriptor.name, name) && descriptor.has_timestamp == with_timestamp;
	};
	auto descriptor = std::find_if(begin(descriptors), end(descriptors), check);
	if (descriptor != end(descriptors))
		return ASDUData { &*descriptor, ioa, ioa_sequence_start };
	else
		return std::nullopt;
}

std::optional<ASDUData> ASDUData::lookupType(int type, int ioa, int ioa_sequence_start)
{
	auto check = [type] (Descriptor descriptor) {
		return descriptor.type == type;
	};
	auto descriptor = std::find_if(begin(descriptors), end(descriptors), check);
	if (descriptor != end(descriptors))
		return ASDUData { &*descriptor, ioa, ioa_sequence_start };
	else
		return std::nullopt;
}

bool ASDUData::hasTimestamp() const
{
	return descriptor->has_timestamp;
}

ASDUData::Type ASDUData::type() const
{
	return descriptor->type;
}


char const * ASDUData::name() const {
	return descriptor->name;
}

ASDUData::Type ASDUData::typeWithoutTimestamp() const
{
	return descriptor->type_without_timestamp;
}

ASDUData ASDUData::withoutTimestamp() const
{
	return ASDUData::lookupType(typeWithoutTimestamp(), ioa, ioa_sequence_start).value();
}

SignalType ASDUData::signalType() const
{
	return descriptor->signal_type;
}

std::optional<ASDUData::Sample> ASDUData::checkASDU(CS101_ASDU const &asdu) const
{
	if (CS101_ASDU_getTypeID(asdu) != static_cast<int> (descriptor->type))
		return std::nullopt;

	for (int i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {
		InformationObject io = CS101_ASDU_getElement(asdu, i);

		if (ioa != InformationObject_getObjectAddress(io)) {
			InformationObject_destroy(io);
			continue;
		}

		SignalData signal_data;
		QualityDescriptor quality;
		switch (typeWithoutTimestamp()) {
			case ASDUData::SCALED_INT: {
				auto scaled = reinterpret_cast<MeasuredValueScaled> (io);
				int value = MeasuredValueScaled_getValue(scaled);
				signal_data.i = static_cast<int64_t> (value);
				quality = MeasuredValueScaled_getQuality(scaled);
			} break;
			case ASDUData::NORMALIZED_FLOAT: {
				auto normalized = reinterpret_cast<MeasuredValueNormalized> (io);
				float value = MeasuredValueNormalized_getValue(normalized);
				signal_data.f = static_cast<double> (value);
				quality = MeasuredValueNormalized_getQuality(normalized);
			} break;
			case ASDUData::DOUBLE_POINT: {
				auto double_point = reinterpret_cast<DoublePointInformation> (io);
				DoublePointValue value = DoublePointInformation_getValue(double_point);
				signal_data.i = static_cast<int64_t> (value);
				quality = DoublePointInformation_getQuality(double_point);
			} break;
			case ASDUData::SINGLE_POINT: {
				auto single_point = reinterpret_cast<SinglePointInformation> (io);
				bool value = SinglePointInformation_getValue(single_point);
				signal_data.b = static_cast<bool> (value);
				quality = SinglePointInformation_getQuality(single_point);
			} break;
			case ASDUData::SHORT_FLOAT: {
				auto short_value = reinterpret_cast<MeasuredValueShort> (io);
				float value = MeasuredValueShort_getValue(short_value);
				signal_data.f = static_cast<double> (value);
				quality = MeasuredValueShort_getQuality(short_value);
			} break;
			default: assert(!"unreachable");
		}

		std::optional<CP56Time2a> time_cp56;
		switch (type()) {
			case ASDUData::SCALED_INT_WITH_TIMESTAMP: {
				auto scaled = reinterpret_cast<MeasuredValueScaledWithCP56Time2a> (io);
				time_cp56 = MeasuredValueScaledWithCP56Time2a_getTimestamp(scaled);
			} break;
			case ASDUData::NORMALIZED_FLOAT_WITH_TIMESTAMP: {
				auto normalized = reinterpret_cast<MeasuredValueNormalizedWithCP56Time2a> (io);
				time_cp56 = MeasuredValueNormalizedWithCP56Time2a_getTimestamp(normalized);
			} break;
			case ASDUData::DOUBLE_POINT_WITH_TIMESTAMP: {
				auto double_point = reinterpret_cast<DoublePointWithCP56Time2a> (io);
				time_cp56 = DoublePointWithCP56Time2a_getTimestamp(double_point);
			} break;
			case ASDUData::SINGLE_POINT_WITH_TIMESTAMP: {
				auto single_point = reinterpret_cast<SinglePointWithCP56Time2a> (io);
				time_cp56 = SinglePointWithCP56Time2a_getTimestamp(single_point);
			} break;
			case ASDUData::SHORT_FLOAT_WITH_TIMESTAMP: {
				auto short_value = reinterpret_cast<MeasuredValueShortWithCP56Time2a> (io);
				time_cp56 = MeasuredValueShortWithCP56Time2a_getTimestamp(short_value);
			} break;
			default: time_cp56 = std::nullopt;
		}

		InformationObject_destroy(io);

		std::optional<timespec> timestamp = time_cp56.has_value()
			? std::optional { cp56time2a_to_timespec(*time_cp56) }
			: std::nullopt;

		return ASDUData::Sample { signal_data, quality, timestamp };
	}

	return std::nullopt;
}

bool ASDUData::addSampleToASDU(CS101_ASDU &asdu, ASDUData::Sample sample) const
{
	std::optional<CP56Time2a> timestamp = sample.timestamp.has_value()
		? std::optional { timespec_to_cp56time2a(sample.timestamp.value()) }
		: std::nullopt;

	InformationObject io;
	switch (descriptor->type) {
	case ASDUData::SCALED_INT: {
		auto value = static_cast<int16_t> (sample.signal_data.i & 0xFFFF);
		auto scaled = MeasuredValueScaled_create(NULL, ioa, value, sample.quality);
		io = reinterpret_cast<InformationObject> (scaled);
	} break;
	case ASDUData::NORMALIZED_FLOAT: {
		auto value = static_cast<float> (sample.signal_data.f);
		auto normalized = MeasuredValueNormalized_create(NULL, ioa, value, sample.quality);
		io = reinterpret_cast<InformationObject> (normalized);
	} break;
	case ASDUData::DOUBLE_POINT: {
		auto value = static_cast<DoublePointValue> (sample.signal_data.i & 0x3);
		auto double_point = DoublePointInformation_create(NULL, ioa, value, sample.quality);
		io = reinterpret_cast<InformationObject> (double_point);
	} break;
	case ASDUData::SINGLE_POINT: {
		auto value = sample.signal_data.b;
		auto single_point = SinglePointInformation_create(NULL, ioa, value, sample.quality);
		io = reinterpret_cast<InformationObject> (single_point);
	} break;
	case ASDUData::SHORT_FLOAT: {
		auto value = static_cast<float> (sample.signal_data.f);
		auto short_float = MeasuredValueShort_create(NULL, ioa, value, sample.quality);
		io = reinterpret_cast<InformationObject> (short_float);
	} break;
	case ASDUData::SCALED_INT_WITH_TIMESTAMP: {
		auto value = static_cast<int16_t> (sample.signal_data.i & 0xFFFF);
		auto scaled = MeasuredValueScaledWithCP56Time2a_create(NULL, ioa, value, sample.quality, timestamp.value());
		io = reinterpret_cast<InformationObject> (scaled);
	} break;
	case ASDUData::NORMALIZED_FLOAT_WITH_TIMESTAMP: {
		auto value = static_cast<float> (sample.signal_data.f);
		auto normalized = MeasuredValueNormalizedWithCP56Time2a_create(NULL, ioa, value, sample.quality, timestamp.value());
		io = reinterpret_cast<InformationObject> (normalized);
	} break;
	case ASDUData::DOUBLE_POINT_WITH_TIMESTAMP: {
		auto value = static_cast<DoublePointValue> (sample.signal_data.i & 0x3);
		auto double_point = DoublePointWithCP56Time2a_create(NULL, ioa, value, sample.quality, timestamp.value());
		io = reinterpret_cast<InformationObject> (double_point);
	} break;
	case ASDUData::SINGLE_POINT_WITH_TIMESTAMP: {
		auto value = sample.signal_data.b;
		auto single_point = SinglePointWithCP56Time2a_create(NULL, ioa, value, sample.quality, timestamp.value());
		io = reinterpret_cast<InformationObject> (single_point);
	} break;
	case ASDUData::SHORT_FLOAT_WITH_TIMESTAMP: {
		auto value = static_cast<float> (sample.signal_data.f);
		auto short_float = MeasuredValueShortWithCP56Time2a_create(NULL, ioa, value, sample.quality, timestamp.value());
		io = reinterpret_cast<InformationObject> (short_float);
	} break;
	default: assert(!"unreachable");
	}
	bool successfully_added = CS101_ASDU_addInformationObject(asdu, io);
	InformationObject_destroy(io);
	return successfully_added;
}

ASDUData::ASDUData(ASDUData::Descriptor const *descriptor, int ioa, int ioa_sequence_start) : ioa(ioa), ioa_sequence_start(ioa_sequence_start), descriptor(descriptor)
{}

void SlaveNode::createSlave() noexcept
{
	// destroy slave id it was already created
	destroySlave();

	// create the slave object
	server.slave = CS104_Slave_create(server.low_priority_queue, server.high_priority_queue);
	CS104_Slave_setServerMode(server.slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);

	// configure the slave according to config
	server.asdu_app_layer_parameters = CS104_Slave_getAppLayerParameters(server.slave);
	CS104_APCIParameters apci_parameters = CS104_Slave_getConnectionParameters(server.slave);

	if (server.apci_t0) apci_parameters->t0 = *server.apci_t0;
	if (server.apci_t1) apci_parameters->t1 = *server.apci_t1;
	if (server.apci_t2) apci_parameters->t2 = *server.apci_t2;
	if (server.apci_t3) apci_parameters->t3 = *server.apci_t3;
	if (server.apci_k) apci_parameters->k = *server.apci_k;
	if (server.apci_w) apci_parameters->w = *server.apci_w;

	CS104_Slave_setLocalAddress(server.slave, server.local_address.c_str());
	CS104_Slave_setLocalPort(server.slave, server.local_port);

	// setup callbacks into the class
	CS104_Slave_setClockSyncHandler(server.slave, [] (void *tcp_node, IMasterConnection connection, CS101_ASDU asdu, CP56Time2a new_time) {
		auto self = static_cast<SlaveNode const *> (tcp_node);
		return self->onClockSync(connection, asdu, new_time);
	}, this);

	CS104_Slave_setInterrogationHandler(server.slave, [] (void *tcp_node, IMasterConnection connection, CS101_ASDU asdu, QualifierOfInterrogation qoi) {
		auto self = static_cast<SlaveNode const *> (tcp_node);
		return self->onInterrogation(connection, asdu, qoi);
	}, this);

	CS104_Slave_setASDUHandler(server.slave, [] (void *tcp_node, IMasterConnection connection, CS101_ASDU asdu) {
		auto self = static_cast<SlaveNode const *> (tcp_node);
		return self->onASDU(connection, asdu);
	}, this);

	CS104_Slave_setConnectionEventHandler(server.slave, [](void *tcp_node, IMasterConnection connection, CS104_PeerConnectionEvent event){
		auto self = static_cast<SlaveNode const *> (tcp_node);
		self->debugPrintConnection(connection, event);
	}, this);

	CS104_Slave_setRawMessageHandler(server.slave, [](void *tcp_node, IMasterConnection connection, uint8_t *message, int message_size, bool sent){
		auto self = static_cast<SlaveNode const *> (tcp_node);
		self->debugPrintMessage(connection, message, message_size, sent);
	}, this);

	server.state = SlaveNode::Server::READY;
}

void SlaveNode::destroySlave() noexcept
{
	if (server.state == SlaveNode::Server::NONE)
		return;

	stopSlave();

	CS104_Slave_destroy(server.slave);
	server.state = SlaveNode::Server::NONE;
}

void SlaveNode::startSlave() noexcept(false)
{
	if (server.state == SlaveNode::Server::NONE)
		createSlave();
	else
		stopSlave();

	server.state = SlaveNode::Server::READY;

	CS104_Slave_start(server.slave);

	if (!CS104_Slave_isRunning(server.slave))
		throw std::runtime_error{"iec60870-5-104 server could not be started"};
}

void SlaveNode::stopSlave() noexcept
{
	if (server.state != SlaveNode::Server::READY || !CS104_Slave_isRunning(server.slave))
		return;

	server.state = SlaveNode::Server::STOPPED;

	if (CS104_Slave_getNumberOfQueueEntries(server.slave, NULL) != 0)
		logger->info("waiting for last messages in queue");
	// wait for all messages to be send before really stopping
	while (	(CS104_Slave_getNumberOfQueueEntries(server.slave, NULL) != 0) &&
		(CS104_Slave_getOpenConnections(server.slave) != 0))
		std::this_thread::sleep_for(100ms);

	CS104_Slave_stop(server.slave);
}

void SlaveNode::debugPrintMessage(IMasterConnection connection, uint8_t* message, int message_size, bool sent) const noexcept
{
	/// ToDo: debug print the message bytes as trace
}

void SlaveNode::debugPrintConnection(IMasterConnection connection, CS104_PeerConnectionEvent event) const noexcept
{
	switch (event) {
	case CS104_CON_EVENT_CONNECTION_OPENED: {
		logger->info("client connected");
	} break;
	case CS104_CON_EVENT_CONNECTION_CLOSED: {
		logger->info("client disconnected");
	} break;
	case CS104_CON_EVENT_ACTIVATED: {
		logger->info("connection activated");
	} break;
	case CS104_CON_EVENT_DEACTIVATED: {
		logger->info("connection closed");
	} break;
	}
}

bool SlaveNode::onClockSync(IMasterConnection connection, CS101_ASDU asdu, CP56Time2a new_time) const noexcept
{
	logger->warn("received clock sync command (unimplemented)");
	return true;
}

bool SlaveNode::onInterrogation(IMasterConnection connection, CS101_ASDU asdu, QualifierOfInterrogation qoi) const noexcept
{
	switch (qoi) {
	// send last values without timestamps
	case IEC60870_QOI_STATION: {
		IMasterConnection_sendACT_CON(connection, asdu, false);

		logger->debug("received general interrogation");

		auto guard = std::lock_guard { output.last_values_mutex };

		for(auto const &asdu_type : output.asdu_types) {
			for (unsigned i = 0; i < output.mapping.size();) {
				auto signal_asdu = CS101_ASDU_create(
					IMasterConnection_getApplicationLayerParameters(connection),
					false,
					CS101_COT_INTERROGATED_BY_STATION,
					0,
					server.common_address,
					false,
					false
				);

				do {
					auto asdu_data = output.mapping[i].withoutTimestamp();
					auto last_value = output.last_values[i];

					if (asdu_data.type() != asdu_type)
						continue;

					if(asdu_data.addSampleToASDU(signal_asdu, ASDUData::Sample {
						last_value,
						IEC60870_QUALITY_GOOD,
						std::nullopt
					}) == false)
						break;
				} while (++i < output.mapping.size());

				IMasterConnection_sendASDU(connection, signal_asdu);

				CS101_ASDU_destroy(signal_asdu);
			}
		}

		IMasterConnection_sendACT_TERM(connection, asdu);
	} break;
	// negative acknowledgement
	default:
		IMasterConnection_sendACT_CON(connection, asdu, true);
		logger->warn("ignoring interrogation type {}", qoi);
	}

	return true;
}

bool SlaveNode::onASDU(IMasterConnection connection, CS101_ASDU asdu) const noexcept
{
	logger->warn("ignoring asdu type {}", CS101_ASDU_getTypeID(asdu));
	return true;
}

void SlaveNode::sendPeriodicASDUsForSample(Sample const *sample) const noexcept(false)
{
	// ASDUs may only carry one type of asdu
	for (auto const &type : output.asdu_types) {
		// search all occurences of this ASDU type
		for (unsigned signal = 0; signal < MIN(sample->length, output.mapping.size());) {
			// create an ASDU for periodic transimission
			CS101_ASDU asdu = CS101_ASDU_create(
				server.asdu_app_layer_parameters,
				0,
				CS101_COT_PERIODIC,
				0,
				server.common_address,
				false,
				false
			);

			do {
				auto &asdu_data = output.mapping[signal];

				// this signal_data does not belong in this ASDU
				if (asdu_data.type() != type)
					continue;

				auto timestamp = (sample->flags & (int) SampleFlags::HAS_TS_ORIGIN)
					? std::optional{ sample->ts.origin }
					: std::nullopt;

				if (asdu_data.hasTimestamp() && !timestamp.has_value())
					throw RuntimeError("Received sample without timestamp for ASDU type with mandatory timestamp");

				if (asdu_data.signalType() != sample_format(sample, signal))
					throw RuntimeError("Expected signal type {}, but received {}",
						signalTypeToString(asdu_data.signalType()),
						signalTypeToString(sample_format(sample, signal))
					);

				if (asdu_data.addSampleToASDU(asdu, ASDUData::Sample {
					sample->data[signal],
					IEC60870_QUALITY_GOOD,
					timestamp
				}) == false)
					// ASDU is full -> dispatch -> create a new one
					break;
			} while (++signal < MIN(sample->length, output.mapping.size()));

			if (CS101_ASDU_getNumberOfElements(asdu) != 0)
			        CS104_Slave_enqueueASDU(server.slave, asdu);

		        CS101_ASDU_destroy(asdu);
		}
	}
}

int SlaveNode::_write(Sample *samples[], unsigned sample_count)
{
	if (server.state != SlaveNode::Server::READY)
		return -1;

	for (unsigned sample_index = 0; sample_index < sample_count; sample_index++) {
		Sample const *sample = samples[sample_index];

		// update last_values
		output.last_values_mutex.lock();
		for (unsigned i = 0; i < MIN(sample->length, output.last_values.size()); i++) {
			output.last_values[i] = sample->data[i];
		}
		output.last_values_mutex.unlock();

		sendPeriodicASDUsForSample(sample);
	}

	return sample_count;
}

SlaveNode::SlaveNode(const std::string &name) :
	Node(name)
{
}

SlaveNode::~SlaveNode()
{
	destroySlave();
}

int SlaveNode::parse(json_t *json, const uuid_t sn_uuid)
{
	int ret = Node::parse(json, sn_uuid);
	if (ret)
		return ret;

	json_error_t err;
	auto signals = getOutputSignals();

	json_t *out_json = nullptr;
	char const *address = nullptr;
	if(json_unpack_ex(json, &err, 0, "{ s?: o, s?: s, s?: i, s?: i, s?: i, s?: i }",
		"out", &out_json,
		"address", &address,
		"port", &server.local_port,
		"ca", &server.common_address,
		"low_priority_queue", &server.low_priority_queue,
		"high_priority_queue", &server.high_priority_queue
	))
		throw ConfigError(json, err, "node-config-node-iec60870-5-104");

	if (address)
		server.local_address = address;

	json_t *signals_json = nullptr;
	int duplicate_ioa_is_sequence = false;
	if (out_json) {
		output.enabled = true;
		if(json_unpack_ex(out_json, &err, 0, "{ s: o, s?: b }",
			"signals", &signals_json,
			"duplicate_ioa_is_sequence", &duplicate_ioa_is_sequence
		))
			throw ConfigError(out_json, err, "node-config-node-iec60870-5-104");
	}

	if (signals_json) {
		json_t *signal_json;
		size_t i;
		std::optional<ASDUData> last_data = std::nullopt;
		json_array_foreach(signals_json, i, signal_json) {
			auto signal = signals ? signals->getByIndex(i) : Signal::Ptr{};
			auto asdu_data = ASDUData::parse(signal_json, last_data, duplicate_ioa_is_sequence);
			last_data = asdu_data;
			SignalData initial_value;
			if (signal) {
				if (signal->type != asdu_data.signalType())
					throw RuntimeError("Type mismatch! Expected type {} for signal {}, but found {}",
						signalTypeToString(asdu_data.signalType()),
						signal->name,
						signalTypeToString(signal->type)
					);
				switch (signal->type) {
				case SignalType::BOOLEAN: {
					initial_value.b = false;
				} break;
				case SignalType::INTEGER: {
					initial_value.i = 0;
				} break;
				case SignalType::FLOAT: {
					initial_value.f = 0;
				} break;
				default: assert(!"unreachable");
				}
			} else
				initial_value.f = 0.0;
			output.mapping.push_back(asdu_data);
			output.last_values.push_back(initial_value);
		}
	}

	for (auto const &asdu_data : output.mapping) {
		if (std::find(begin(output.asdu_types), end(output.asdu_types), asdu_data.type()) == end(output.asdu_types))
			output.asdu_types.push_back(asdu_data.type());
	}

	return 0;
}

int SlaveNode::start()
{
	startSlave();
	return Node::start();
}

int SlaveNode::stop()
{
	stopSlave();
	return Node::stop();
}

static char name[] = "iec60870-5-104";
static char description[] = "Provide values as protocol slave";
static NodePlugin<SlaveNode, name, description, (int) NodeFactory::Flags::SUPPORTS_WRITE> p;
