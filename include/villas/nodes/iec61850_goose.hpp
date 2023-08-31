/* Node type: IEC 61850 - GOOSE
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <array>
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <map>
#include <optional>
#include <string>
#include <thread>

#include <libiec61850/goose_receiver.h>
#include <libiec61850/goose_subscriber.h>
#include <libiec61850/goose_publisher.h>

#include <villas/node/config.hpp>
#include <villas/node.hpp>
#include <villas/pool.hpp>
#include <villas/queue_signalled.h>
#include <villas/signal.hpp>

namespace villas {
namespace node {
namespace iec61850 {

// A GooseSignal is a SignalData value with attached Metadata for the MmsType and SignalType
class GooseSignal {
public:
	union Meta {
		int size;
	};

	struct Descriptor {
		std::string name;
		SignalType signal_type;
		MmsType mms_type;
		Meta default_meta;
	};

	using Type = Descriptor const *;

	// The config file identifier for this type
	std::string const & name() const;

	// The type of this value
	Type type() const;

	// Corresponding mms type
	MmsType mmsType() const;

	// Corresponding signal type
	SignalType signalType() const;

	// Create a GooseSignal from an MmsValue
	static
	std::optional<GooseSignal> fromMmsValue(MmsValue *mms_value);

	// Create a GooseSignal from type name and SignalData value
	static
	std::optional<GooseSignal> fromNameAndValue(char const *name, SignalData value, std::optional<Meta> meta = std::nullopt);

	// Create a MmsValue from this GooseSignal
	MmsValue * toMmsValue() const;

	static
	std::optional<Type> lookupMmsType(int mms_type);

	static
	std::optional<Type> lookupMmsTypeName(char const *name);

	GooseSignal(Type type, SignalData value, std::optional<Meta> meta = std::nullopt);

	SignalData signal_data;
	Meta meta;
private:
	inline static
	std::array const descriptors {
		Descriptor { "boolean", 	SignalType::BOOLEAN, 	MmsType::MMS_BOOLEAN },
		Descriptor { "int8",		SignalType::INTEGER,	MmsType::MMS_INTEGER,		{.size =  8 } },
		Descriptor { "int16",		SignalType::INTEGER,	MmsType::MMS_INTEGER,		{.size = 16 } },
		Descriptor { "int32",		SignalType::INTEGER,	MmsType::MMS_INTEGER,		{.size = 32 } },
		Descriptor { "int64",		SignalType::INTEGER,	MmsType::MMS_INTEGER,		{.size = 64 } },
		Descriptor { "int8u",		SignalType::INTEGER,	MmsType::MMS_UNSIGNED,		{.size =  8 } },
		Descriptor { "int16u",		SignalType::INTEGER,	MmsType::MMS_UNSIGNED,		{.size = 16 } },
		Descriptor { "int32u",		SignalType::INTEGER,	MmsType::MMS_UNSIGNED,		{.size = 32 } },
		Descriptor { "bitstring",	SignalType::INTEGER,	MmsType::MMS_BIT_STRING,	{.size = 32 } },
		Descriptor { "float32",		SignalType::FLOAT,	MmsType::MMS_FLOAT,		{.size = 32 } },
		Descriptor { "float64",		SignalType::FLOAT,	MmsType::MMS_FLOAT,		{.size = 64 } },
	};

	static
	MmsValue * newMmsInteger(int64_t i, int size);

	static
	MmsValue * newMmsUnsigned(uint64_t i, int size);

	static
	MmsValue * newMmsBitString(uint32_t i, int size);

	static
	MmsValue * newMmsFloat(double i, int size);

	// Descriptor within the descriptors table above
	Descriptor const *descriptor;
};

bool operator==(GooseSignal &lhs, GooseSignal &rhs);
bool operator!=(GooseSignal &lhs, GooseSignal &rhs);

class GooseNode : public Node {
protected:
	enum InputTrigger {
		CHANGE,
		ALWAYS,
	};

	struct InputMapping {
		std::string subscriber;
		unsigned int index;
		GooseSignal::Type type;
	};

	struct SubscriberConfig {
		std::string go_cb_ref;
		InputTrigger trigger;
		std::optional<std::array<uint8_t, 6>> dst_address;
		std::optional<uint16_t> app_id;
	};

	struct InputEventContext {
		SubscriberConfig subscriber_config;

		GooseNode *node;
		std::vector<std::optional<GooseSignal>> values;
		int last_state_num;
	};

	struct Input {
		enum { NONE, STOPPED, READY } state;
		GooseReceiver receiver;
		CQueueSignalled queue;
		Pool pool;

		std::map<std::string, InputEventContext> contexts;
		std::vector<InputMapping> mappings;
		std::string interface_id;
		bool with_timestamp;
		unsigned int queue_length;
	} input;

	struct OutputData {
		std::optional<unsigned int> signal;
		GooseSignal default_value;
	};

	struct PublisherConfig {
		std::string go_id;
		std::string go_cb_ref;
		std::string data_set_ref;
		std::array<uint8_t, 6> dst_address;
		uint16_t app_id;
		uint32_t conf_rev;
		uint32_t time_allowed_to_live;
		int burst;
		std::vector<OutputData> data;
	};

	struct OutputContext {
		PublisherConfig config;
		std::vector<GooseSignal> values;

		GoosePublisher publisher;
	};

	struct Output {
		enum { NONE, STOPPED, READY } state;
		std::vector<OutputContext> contexts;
		std::string interface_id;
		double resend_interval;

		std::mutex send_mutex;
		bool changed;
		bool resend_thread_stop;
		std::optional<std::thread> resend_thread;
		std::condition_variable resend_thread_cv;
	} output;

	void createReceiver() noexcept;
	void destroyReceiver() noexcept;

	void startReceiver() noexcept(false);
	void stopReceiver() noexcept;

	void createPublishers() noexcept;
	void destroyPublishers() noexcept;

	void startPublishers() noexcept(false);
	void stopPublishers() noexcept;

	static
	void onEvent(GooseSubscriber subscriber, InputEventContext &context) noexcept;

	void addSubscriber(InputEventContext &ctx) noexcept;
	void pushSample(uint64_t timestamp) noexcept;

	static
	void publish_values(GoosePublisher publisher, std::vector<GooseSignal> &values, bool changed, int burst = 1) noexcept;
	static
	void resend_thread(GooseNode::Output *output) noexcept;

	void parseInput(json_t *json);
	void parseSubscriber(json_t *json, SubscriberConfig &sc);
	void parseSubscribers(json_t *json, std::map<std::string, InputEventContext> &ctx);
	void parseInputSignals(json_t *json, std::vector<InputMapping> &mappings);

	void parseOutput(json_t *json);
	void parsePublisherData(json_t *json, std::vector<OutputData> &data);
	void parsePublisher(json_t *json, PublisherConfig &pc);
	void parsePublishers(json_t *json, std::vector<OutputContext> &ctx);

	virtual
	int _read(struct Sample *smps[], unsigned cnt) override;

	virtual
	int _write(struct Sample *smps[], unsigned cnt) override;

public:
	GooseNode(const uuid_t &id = {}, const std::string &name = "");

	virtual
	~GooseNode() override;

	virtual
	std::vector<int> getPollFDs() override;

	virtual
	int parse(json_t *json) override;

	virtual
	int prepare() override;

	virtual
	int start() override;

	virtual
	int stop() override;
};

} // namespace iec61850
} // namespace node
} // namespace villas
