/** Node type: IEC 61850 - GOOSE
 *
 * @author Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * @copyright 2023, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <algorithm>
#include <villas/node_compat.hpp>
#include <villas/nodes/iec61850_goose.hpp>
#include <villas/utils.hpp>
#include <villas/sample.hpp>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::node::iec61850;
using namespace std::literals::chrono_literals;

static std::optional<std::array<uint8_t, 6>> stringToMac(char *mac_string) {
	std::array<uint8_t, 6> mac;
	char *save;
	char *token = strtok_r(mac_string, ":", &save);

	for (auto &i : mac) {
		if (!token) return std::nullopt;

		i = static_cast<uint8_t>(strtol(token, NULL, 16));

		token = strtok_r(NULL, ":", &save);
	}

	return std::optional { mac };
}

std::optional<MmsType> GooseValue::mmsType() const
{
	return descriptor->mms_type;
}

const char* GooseValue::name() const
{
	return descriptor->name;
}

SignalType GooseValue::signalType() const
{
	return descriptor->signal_type;
}

GooseValue GooseValue::fromMmsValue(MmsValue *mms_value) {
	auto mms_type = MmsValue_getType(mms_value);
	auto descriptor = lookupMmsType(mms_type);
	SignalData data;

	if (!descriptor) return {};

	switch (mms_type) {
	case MmsType::MMS_BOOLEAN:
		data.b = MmsValue_getBoolean(mms_value);
		break;
	case MmsType::MMS_INTEGER:
		data.i = MmsValue_toInt64(mms_value);
		break;
	case MmsType::MMS_UNSIGNED:
		data.i = static_cast<int64_t>(MmsValue_toUint32(mms_value));
		break;
	case MmsType::MMS_BIT_STRING:
		data.i = static_cast<int64_t>(MmsValue_getBitStringAsInteger(mms_value));
		break;
	case MmsType::MMS_FLOAT:
		data.f = MmsValue_toDouble(mms_value);
		break;
	default:
		return {};
	}

	return GooseValue { descriptor.value(), data };
}

GooseValue GooseValue::fromNameAndValue(char *name, SignalData value) {
	auto descriptor = lookupName(name);

	if (!descriptor) return {};

	return GooseValue { descriptor.value(), value };
}

std::optional<MmsValue *> GooseValue::toMmsValue() const {
	if (!descriptor->mms_type) return std::nullopt;

	switch (*descriptor->mms_type) {
	case MmsType::MMS_BOOLEAN:
		return MmsValue_newBoolean(signal_data.b);
	case MmsType::MMS_INTEGER:
		return newMmsInteger(signal_data.i, meta.size);
	case MmsType::MMS_UNSIGNED:
		return newMmsUnsigned(static_cast<uint64_t>(signal_data.i), meta.size);
	case MmsType::MMS_FLOAT:
		return newMmsFloat(signal_data.f, meta.size);
	default:
		assert(!"unreachable");
	}
}

MmsValue * GooseValue::newMmsInteger(int64_t i, GooseValue::MetaSize size) {
	auto mms_integer = MmsValue_newInteger(size.value);

	switch (size.value) {
	case 8:
		MmsValue_setInt8(mms_integer, static_cast<int8_t>(i));
		return mms_integer;
	case 16:
		MmsValue_setInt16(mms_integer, static_cast<int16_t>(i));
		return mms_integer;
	case 32:
		MmsValue_setInt32(mms_integer, static_cast<int32_t>(i));
		return mms_integer;
	case 64:
		MmsValue_setInt64(mms_integer, static_cast<int64_t>(i));
		return mms_integer;
	default:
		assert(!"unreachable");
	}
}

MmsValue * GooseValue::newMmsUnsigned(uint64_t u, GooseValue::MetaSize size) {
	auto mms_unsigned = MmsValue_newUnsigned(size.value);

	switch (size.value) {
	case 8:
		MmsValue_setUint8(mms_unsigned, static_cast<uint8_t>(u));
		return mms_unsigned;
	case 16:
		MmsValue_setUint16(mms_unsigned, static_cast<uint16_t>(u));
		return mms_unsigned;
	case 32:
		MmsValue_setUint32(mms_unsigned, static_cast<uint32_t>(u));
		return mms_unsigned;
	default:
		assert(!"unreachable");
	}
}

MmsValue * GooseValue::newMmsFloat(double d, GooseValue::MetaSize size) {
	switch (size.value) {
	case 32:
		return MmsValue_newFloat(static_cast<float>(d));
	case 64:
		return MmsValue_newDouble(d);
	default:
		assert(!"unreachable");
	}
}

std::optional<GooseValue::Descriptor const *> GooseValue::lookupMmsType(int mms_type)
{
	auto check = [mms_type] (Descriptor descriptor) {
		return descriptor.mms_type == mms_type;
	};

	auto descriptor = std::find_if(begin(descriptors), end(descriptors), check);
	if (descriptor != end(descriptors))
		return &*descriptor;
	else
		return std::nullopt;
}

std::optional<GooseValue::Descriptor const *> GooseValue::lookupName(char *name)
{
	auto check = [name] (Descriptor descriptor) {
		return !strcmp(descriptor.name, name);
	};

	auto descriptor = std::find_if(begin(descriptors), end(descriptors), check);
	if (descriptor != end(descriptors))
		return &*descriptor;
	else
		return std::nullopt;
}

GooseValue::GooseValue()
: signal_data({})
, descriptor(&descriptors[0])
{
}

GooseValue::GooseValue(GooseValue::Descriptor const *descriptor, SignalData data)
: signal_data(data)
, descriptor(descriptor)
{
}

void GooseNode::onEvent(GooseSubscriber subscriber, GooseNode::InputEventContext &ctx) noexcept
{
	if (!GooseSubscriber_isValid(subscriber) || GooseSubscriber_needsCommission(subscriber))
		return;

	if (	ctx.subscriber_config.trigger == InputTrigger::CHANGE
	&&	!ctx.values.empty()
	&&	GooseSubscriber_getSqNum(subscriber) != 0)
		return;

	auto mms_values = GooseSubscriber_getDataSetValues(subscriber);

	if (MmsValue_getType(mms_values) != MmsType::MMS_ARRAY) {
		ctx.node->logger->warn("DataSet is not an array");
		return;
	}

	ctx.values.resize(MmsValue_getArraySize(mms_values));

	for (unsigned int i = 0; i < MmsValue_getArraySize(mms_values); i++) {
		auto mms_value = MmsValue_getElement(mms_values, i);
		auto goose_value = GooseValue::fromMmsValue(mms_value);
		ctx.values[i] = goose_value;
	}

	uint64_t timestamp = GooseSubscriber_getTimestamp(subscriber);

	ctx.node->pushSample(timestamp);
}

void GooseNode::pushSample(uint64_t timestamp) noexcept
{
	Sample *sample = sample_alloc(&input.pool);
	if (!sample) {
		logger->warn("Pool underrun");
		return;
	}


	sample->length = input.mappings.size();
	sample->flags = (int) SampleFlags::HAS_DATA;
	sample->signals = getInputSignals(false);

	if (input.with_timestamp) {
		sample->flags |= (int) SampleFlags::HAS_TS_ORIGIN;
		sample->ts.origin.tv_sec = timestamp / 1000;
		sample->ts.origin.tv_nsec = 1000 * (timestamp % 1000);
	}

	for (unsigned int signal = 0; signal < sample->length; signal++) {
		auto& mapping = input.mappings[signal];
		auto& values = input.contexts[mapping.subscriber].values;
		sample->data[signal] = mapping.init.signal_data;

		if (mapping.index >= values.size())
			continue;

		if (mapping.init.mmsType() != values[mapping.index].mmsType()) {
			logger->error("unexpected mms_type for signal {}", sample->signals->getByIndex(signal)->toString());
			continue;
		}

		sample->data[signal] = values[mapping.index].signal_data;
	}

	if (queue_signalled_push(&input.queue, sample) != 1)
		logger->warn("Failed to enqueue samples");
}

void GooseNode::addSubscriber(GooseNode::InputEventContext &ctx) noexcept
{
	GooseSubscriber subscriber;
	SubscriberConfig &sc = ctx.subscriber_config;

	ctx.node = this;

	subscriber = GooseSubscriber_create(sc.go_cb_ref.data(), NULL);

	if (sc.dst_address)
		GooseSubscriber_setDstMac(subscriber, sc.dst_address->data());

	if (sc.app_id)
		GooseSubscriber_setAppId(subscriber, *sc.app_id);

	GooseSubscriber_setListener(subscriber, [] (GooseSubscriber goose_subscriber, void* event_context) {
		auto context = static_cast<InputEventContext *> (event_context);
		onEvent(goose_subscriber, *context);
	}, &ctx);

	GooseReceiver_addSubscriber(input.receiver, subscriber);
}

void GooseNode::createReceiver() noexcept
{
	destroyReceiver();

	input.receiver = GooseReceiver_create();

	GooseReceiver_setInterfaceId(input.receiver, input.interface_id.c_str());

	for (auto& pair_key_context : input.contexts)
		addSubscriber(pair_key_context.second);

	input.state = GooseNode::Input::READY;
}

void GooseNode::destroyReceiver() noexcept
{
	int err __attribute__((unused));

	if (input.state == GooseNode::Input::NONE)
		return;

	stopReceiver();

	GooseReceiver_destroy(input.receiver);

	err = queue_signalled_destroy(&input.queue);

	input.state = GooseNode::Input::NONE;
}

void GooseNode::startReceiver() noexcept(false)
{
	if (input.state == GooseNode::Input::NONE)
		createReceiver();
	else
		stopReceiver();

	GooseReceiver_start(input.receiver);

	if (!GooseReceiver_isRunning(input.receiver))
		throw RuntimeError{"iec61850-GOOSE receiver could not be started"};

	input.state = GooseNode::Input::READY;
}

void GooseNode::stopReceiver() noexcept
{
	if (input.state == GooseNode::Input::NONE)
		return;

	input.state = GooseNode::Input::STOPPED;

	if (!GooseReceiver_isRunning(input.receiver))
		return;

	GooseReceiver_stop(input.receiver);
}

int GooseNode::_read(Sample *samples[], unsigned sample_count)
{
	int available_samples;
	struct Sample *copies[sample_count];

	if (input.state != GooseNode::Input::READY)
		return -1;

	available_samples = queue_signalled_pull_many(&input.queue, (void **) copies, sample_count);
	sample_copy_many(samples, copies, available_samples);
	sample_decref_many(copies, available_samples);

	return available_samples;
}

GooseNode::GooseNode(const std::string &name) :
	Node(name)
{
	input.enabled = false;
	input.state = GooseNode::Input::NONE;

	input.contexts = {};
	input.mappings = {};
	input.interface_id = "lo";
	input.queue_length = 1024;
}

GooseNode::~GooseNode()
{
	int err __attribute__((unused));

	destroyReceiver();

	err = queue_signalled_destroy(&input.queue);

	err = pool_destroy(&input.pool);
}

int GooseNode::parse(json_t *json, const uuid_t sn_uuid)
{
	int ret;
	json_error_t err;

	ret = Node::parse(json, sn_uuid);
	if (ret)
		return ret;

	ret = _parse(json, &err);
	if (ret)
		throw ConfigError(json, err, "node-config-node-iec61850-goose");

	return 0;
}


int GooseNode::_parse(json_t *json, json_error_t *err)
{
	int ret;

	json_t *in_json = nullptr;
	ret = json_unpack_ex(json, err, 0, "{ s: o }",
		"in", &in_json
	);
	if (ret) return ret;

	json_t *subscribers_json = nullptr;
	json_t *in_signals_json = nullptr;
	char const *in_interface_id = nullptr;
	int in_with_timestamp = true;
	ret = json_unpack_ex(in_json, err, 0, "{ s: o, s: o, s?: s, s: b }",
		"subscribers", &subscribers_json,
		"signals", &in_signals_json,
		"interface", &in_interface_id,
		"with_timestamp", &in_with_timestamp
	);
	if (ret) return ret;

	ret = parseSubscribers(subscribers_json, err, input.contexts);
	if (ret) return ret;

	ret = parseInputSignals(in_signals_json, err, input.mappings);
	if (ret) return ret;

	input.interface_id = in_interface_id
		? std::string { in_interface_id }
		: std::string { "eth0" };

	input.with_timestamp = in_with_timestamp;

	return 0;
}

int GooseNode::parseSubscriber(json_t *json, json_error_t *err, GooseNode::SubscriberConfig &sc)
{
	int ret;

	char *go_cb_ref = nullptr;
	char *dst_address_str = nullptr;
	char *trigger = nullptr;
	int app_id = 0;
	ret = json_unpack_ex(json, err, 0, "{ s: s, s?: s, s?: s, s?: i }",
		"go_cb_ref", &go_cb_ref,
		"trigger", &trigger,
		"dst_address", &dst_address_str,
		"app_id", &app_id
	);
	if (ret) return ret;

	sc.go_cb_ref = std::string { go_cb_ref };

	if (!trigger || !strcmp(trigger, "always"))
		sc.trigger = InputTrigger::ALWAYS;
	else if (!strcmp(trigger, "change"))
		sc.trigger = InputTrigger::CHANGE;
	else
		throw RuntimeError("Unknown trigger type");

	if (dst_address_str) {
		std::optional dst_address = stringToMac(dst_address_str);
		if (!dst_address)
			throw RuntimeError("Invalid dst_address");
		sc.dst_address = *dst_address;
	}

	if (app_id != 0)
		sc.app_id = static_cast<int16_t>(app_id);

	return 0;
}

int GooseNode::parseSubscribers(json_t *json, json_error_t *err, std::map<std::string, InputEventContext> &ctx)
{
	int ret;
	char const* key;
	json_t* subscriber_json;

	json_object_foreach(json, key, subscriber_json) {
		SubscriberConfig sc;

		ret = parseSubscriber(subscriber_json, err, sc);
		if (ret) return ret;

		ctx[key] = InputEventContext { sc };
	}

	return 0;
}

int GooseNode::parseInputSignals(json_t *json, json_error_t *err, std::vector<GooseNode::InputMapping> &mappings)
{
	int ret;
	int index;
	json_t *value;

	mappings.clear();

	auto signals = getInputSignals();

	json_array_foreach(json, index, value) {
		auto signal = signals->getByIndex(index);

		char *mapping_subscriber;
		unsigned int mapping_index;
		char *mapping_type;
		ret = json_unpack_ex(value, err, 0, "{ s: s, s: i, s: s }",
			"subscriber", &mapping_subscriber,
			"index", &mapping_index,
			"mms_type", &mapping_type
		);
		if (ret) return ret;

		auto mapping_init = GooseValue::fromNameAndValue(mapping_type, signal->init);
		if (mapping_init.signalType() == SignalType::INVALID)
			throw RuntimeError("Invalid mms_type {}", mapping_type);

		if (signal->type != mapping_init.signalType())
			throw RuntimeError("Expected signal type {} for mms_type {}, but got {}",
				signalTypeToString(mapping_init.signalType()),
				mapping_type,
				signalTypeToString(signal->type));

		mappings.push_back(InputMapping {
			mapping_subscriber,
			mapping_index,
			mapping_init
		});
	}

	return 0;
}

std::vector<int> GooseNode::getPollFDs()
{
	return { queue_signalled_fd(&input.queue) };
}

int GooseNode::prepare()
{
	int ret;

	ret = pool_init(&input.pool, input.queue_length, SAMPLE_LENGTH(getInputSignals(false)->size()));
	if (ret)
		return ret;

	ret = queue_signalled_init(&input.queue, input.queue_length);
	if (ret)
		return ret;

	return Node::prepare();
}

int GooseNode::start()
{
	startReceiver();

	return Node::start();
}

int GooseNode::stop()
{
	int err __attribute__((unused));

	stopReceiver();

	err = queue_signalled_close(&input.queue);

	return Node::stop();
}

static char name[] = "iec61850-goose";
static char description[] = "Subscribe to and publish GOOSE messages";
static NodePlugin<GooseNode, name, description, (int) NodeFactory::Flags::SUPPORTS_READ, 1> p;
