/** Node type: IEC 61850 - GOOSE
 *
 * @author Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * @copyright 2023, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <ctime>
#include <array>
#include <map>
#include <villas/node/config.hpp>
#include <villas/node.hpp>
#include <villas/pool.hpp>
#include <villas/queue_signalled.h>
#include <villas/signal.hpp>
#include <libiec61850/goose_receiver.h>
#include <libiec61850/goose_subscriber.h>

namespace villas {
namespace node {
namespace iec61850 {

// A GooseValue is a SignalData value with attached Metadata for the MmsType and SignalType
class GooseValue {
public:
	struct MetaSize { int value; };

	// The config file identifier for this type
	char const * name() const;

	// Corresponding mms type
	std::optional<MmsType> mmsType() const;

	// Corresponding signal type
	SignalType signalType() const;

	// Create a GooseValue from an MmsValue
	static GooseValue fromMmsValue(MmsValue *mms_value);

	// Create a GooseValue from type name and SignalData value
	static GooseValue fromNameAndValue(char *name, SignalData value);

	// Create a MmsValue from this GooseValue
	std::optional<MmsValue *> toMmsValue() const;

	// Default/Invalid GooseValue
	GooseValue();

	union Meta {
		MetaSize size;
	} meta;

	SignalData signal_data;

private:
	struct Descriptor {
		SignalType signal_type;
		char const *name;
		std::optional<MmsType> mms_type;
		Meta default_meta;
	};

	inline static std::array const descriptors {
		Descriptor { SignalType::INVALID,	"invalid" },

		/* boolean signals */
		Descriptor { SignalType::BOOLEAN,	"boolean",	MmsType::MMS_BOOLEAN },

		/* integer signals */
		Descriptor { SignalType::INTEGER,	"integer",	MmsType::MMS_INTEGER,	 	MetaSize { 64 } },
		Descriptor { SignalType::INTEGER,	"unsigned",	MmsType::MMS_UNSIGNED,	 	MetaSize { 32 } },
		Descriptor { SignalType::INTEGER,	"bit-string",	MmsType::MMS_BIT_STRING,	MetaSize { 32 } },

		/* float signals */
		Descriptor { SignalType::FLOAT,		"float",	MmsType::MMS_FLOAT,		MetaSize { 64 } },
	};

	static MmsValue * newMmsInteger(int64_t i, MetaSize meta);

	static MmsValue * newMmsUnsigned(uint64_t i, MetaSize meta);

	static MmsValue * newMmsFloat(double i, MetaSize meta);

	static std::optional<Descriptor const *> lookupMmsType(int mms_type);

	static std::optional<Descriptor const *> lookupName(char *name);

	GooseValue(Descriptor const *descriptor, SignalData value);

	// Descriptor within the descriptors table above
	Descriptor const *descriptor;
};

class GooseNode : public Node {
protected:
	enum InputTrigger {
		CHANGE,
		ALWAYS,
	};

	struct InputMapping {
		std::string subscriber;
		unsigned int index;
		GooseValue init;
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
		std::vector<GooseValue> values;
	};

	struct Input {
		bool enabled;
		enum { NONE, STOPPED, READY } state;
		GooseReceiver receiver;
		CQueueSignalled queue;
		Pool pool;

		std::map<std::string, InputEventContext> contexts;
		std::vector<InputMapping> mappings;
		std::string interface_id;
		bool with_timestamp;
		unsigned int queue_length;
		int signal_count;
	} input;

	void createReceiver() noexcept;
	void destroyReceiver() noexcept;

	void startReceiver() noexcept(false);
	void stopReceiver() noexcept;

	static void onEvent(GooseSubscriber subscriber, InputEventContext &context) noexcept;

	void addSubscriber(InputEventContext &ctx) noexcept;
	void pushSample(uint64_t timestamp) noexcept;

	int _parse(json_t *json, json_error_t *err);
	int parseSubscriber(json_t *json, json_error_t *err, SubscriberConfig &sc);
	int parseSubscribers(json_t *json, json_error_t *err, std::map<std::string, InputEventContext> &ctx);
	int parseInputSignals(json_t *json, json_error_t *err, std::vector<InputMapping> &mappings);

	virtual
	int _read(struct Sample *smps[], unsigned cnt) override;

public:
	GooseNode(const std::string &name = "");

	virtual
	~GooseNode() override;

	virtual
	std::vector<int> getPollFDs() override;

	virtual
	int parse(json_t *json, const uuid_t sn_uuid) override;

	virtual
	int prepare() override;

	virtual
	int start() override;

	virtual
	int stop() override;
};

} /* namespace iec61850 */
} /* namespace node */
} /* namespace villas */
