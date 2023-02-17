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
using namespace std::literals::string_literals;

static std::optional<std::array<uint8_t, 6>> stringToMac(char *mac_string)
{
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

std::optional<MmsType> GooseSignal::mmsType() const
{
	return descriptor->mms_type;
}

std::string const & GooseSignal::name() const
{
	return descriptor->name;
}

SignalType GooseSignal::signalType() const
{
	return descriptor->signal_type;
}

std::optional<GooseSignal> GooseSignal::fromMmsValue(MmsValue *mms_value)
{
	auto mms_type = MmsValue_getType(mms_value);
	auto descriptor = lookupMmsType(mms_type);
	SignalData data;

	if (!descriptor) return std::nullopt;

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
		return std::nullopt;
	}

	return GooseSignal { descriptor.value(), data };
}

GooseSignal GooseSignal::fromNameAndValue(char const *name, SignalData value, std::optional<Meta> meta)
{
	auto descriptor = lookupMmsTypeName(name);

	if (!descriptor) return {};

	return GooseSignal { descriptor.value(), value, meta };
}

std::optional<MmsValue *> GooseSignal::toMmsValue() const
{
	if (!descriptor->mms_type) return std::nullopt;

	switch (*descriptor->mms_type) {
	case MmsType::MMS_BOOLEAN:
		return MmsValue_newBoolean(signal_data.b);
	case MmsType::MMS_INTEGER:
		return newMmsInteger(signal_data.i, meta.size);
	case MmsType::MMS_UNSIGNED:
		return newMmsUnsigned(static_cast<uint64_t>(signal_data.i), meta.size);
	case MmsType::MMS_BIT_STRING:
		return newMmsBitString(static_cast<uint32_t>(signal_data.i), meta.size);
	case MmsType::MMS_FLOAT:
		return newMmsFloat(signal_data.f, meta.size);
	default:
		assert(!"unreachable");
	}
}

MmsValue * GooseSignal::newMmsBitString(uint32_t i, int size)
{
	auto mms_bit_string = MmsValue_newBitString(size);

	MmsValue_setBitStringFromInteger(mms_bit_string, i);

	return mms_bit_string;
}

MmsValue * GooseSignal::newMmsInteger(int64_t i, int size)
{
	auto mms_integer = MmsValue_newInteger(size);

	switch (size) {
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

MmsValue * GooseSignal::newMmsUnsigned(uint64_t u, int size)
{
	auto mms_unsigned = MmsValue_newUnsigned(size);

	switch (size) {
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

MmsValue * GooseSignal::newMmsFloat(double d, int size)
{
	switch (size) {
	case 32:
		return MmsValue_newFloat(static_cast<float>(d));
	case 64:
		return MmsValue_newDouble(d);
	default:
		assert(!"unreachable");
	}
}

std::optional<GooseSignal::Type> GooseSignal::lookupMmsType(int mms_type)
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

std::optional<GooseSignal::Type> GooseSignal::lookupMmsTypeName(char const *name)
{
	auto check = [name] (Descriptor descriptor) {
		return descriptor.name == name;
	};

	auto descriptor = std::find_if(begin(descriptors), end(descriptors), check);
	if (descriptor != end(descriptors))
		return &*descriptor;
	else
		return std::nullopt;
}

GooseSignal::GooseSignal() :
	signal_data({}),
	descriptor(&descriptors[0])
{
}

GooseSignal::GooseSignal(GooseSignal::Descriptor const *descriptor, SignalData data, std::optional<Meta> meta) :
	signal_data(data),
	meta(meta.value_or(descriptor->default_meta)),
	descriptor(descriptor)
{
}

void GooseNode::onEvent(GooseSubscriber subscriber, GooseNode::InputEventContext &ctx) noexcept
{
	if (!GooseSubscriber_isValid(subscriber) || GooseSubscriber_needsCommission(subscriber))
		return;

	if (ctx.subscriber_config.trigger == InputTrigger::CHANGE
	&& !ctx.values.empty()
	&& GooseSubscriber_getSqNum(subscriber) != 0)
		return;

	auto mms_values = GooseSubscriber_getDataSetValues(subscriber);

	if (MmsValue_getType(mms_values) != MmsType::MMS_ARRAY) {
		ctx.node->logger->warn("DataSet is not an array");
		return;
	}

	ctx.values.resize(MmsValue_getArraySize(mms_values));

	for (unsigned int i = 0; i < MmsValue_getArraySize(mms_values); i++) {
		auto mms_value = MmsValue_getElement(mms_values, i);
		auto goose_value = GooseSignal::fromMmsValue(mms_value).value();
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
		sample->data[signal] = mapping.dflt.signal_data;

		if (mapping.index >= values.size())
			continue;

		if (mapping.dflt.mmsType() != values[mapping.index].mmsType()) {
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

	input.state = Input::READY;
}

void GooseNode::destroyReceiver() noexcept
{
	int err __attribute__((unused));

	if (input.state == Input::NONE)
		return;

	stopReceiver();

	GooseReceiver_destroy(input.receiver);

	err = queue_signalled_destroy(&input.queue);

	input.state = Input::NONE;
}

void GooseNode::startReceiver() noexcept(false)
{
	if (input.state == Input::NONE)
		createReceiver();
	else
		stopReceiver();

	GooseReceiver_start(input.receiver);

	if (!GooseReceiver_isRunning(input.receiver))
		throw RuntimeError{"iec61850-GOOSE receiver could not be started"};

	input.state = Input::READY;
}

void GooseNode::stopReceiver() noexcept
{
	if (input.state == Input::NONE)
		return;

	input.state = Input::STOPPED;

	if (!GooseReceiver_isRunning(input.receiver))
		return;

	GooseReceiver_stop(input.receiver);
}

void GooseNode::createPublishers() noexcept
{
	destroyPublishers();

	for (auto &ctx : output.contexts) {
		auto dst_address = ctx.config.dst_address;
		auto comm = CommParameters {
			/* vlanPriority */ 0,
			/* vlanId */ 0,
			ctx.config.app_id,
			{}
		};

		memcpy(comm.dstAddress, dst_address.data(), dst_address.size());

		ctx.publisher = GoosePublisher_createEx(&comm, output.interface_id.c_str(), false);

		GoosePublisher_setGoID(ctx.publisher, ctx.config.go_id.data());
		GoosePublisher_setGoCbRef(ctx.publisher, ctx.config.go_cb_ref.data());
		GoosePublisher_setDataSetRef(ctx.publisher, ctx.config.data_set_ref.data());
		GoosePublisher_setConfRev(ctx.publisher, ctx.config.conf_rev);
		GoosePublisher_setTimeAllowedToLive(ctx.publisher, ctx.config.time_allowed_to_live);
	}

	output.state = Output::READY;
}

void GooseNode::destroyPublishers() noexcept
{
	int err __attribute__((unused));

	if (output.state == Output::NONE)
		return;

	stopPublishers();

	for (auto &ctx : output.contexts)
		GoosePublisher_destroy(ctx.publisher);

	output.state = Output::NONE;
}

void GooseNode::startPublishers() noexcept(false)
{
	if (output.state == Output::NONE)
		createPublishers();
	else
		stopPublishers();

	output.state = Output::READY;
}

void GooseNode::stopPublishers() noexcept
{
	if (output.state == Output::NONE)
		return;

	output.state = Output::STOPPED;
}

int GooseNode::_read(Sample *samples[], unsigned sample_count)
{
	int available_samples;
	struct Sample *copies[sample_count];

	if (input.state != Input::READY)
		return 0;

	available_samples = queue_signalled_pull_many(&input.queue, (void **) copies, sample_count);
	sample_copy_many(samples, copies, available_samples);
	sample_decref_many(copies, available_samples);

	return available_samples;
}

int GooseNode::_write(Sample *samples[], unsigned sample_count)
{
	if (output.state != Output::READY)
		return 0;

	for (unsigned int i = 0; i < sample_count; i++) {
		auto sample = samples[i];

		for (auto &ctx : output.contexts) {
			auto data_set = LinkedList_create();

			for (auto &data : ctx.config.data) {
				auto goose_value = data.default_value;
				auto signal = data.signal.value_or(-1);
				if (signal >= 0 && signal < (int) sample->length)
					goose_value.signal_data = sample->data[signal];

				LinkedList_add(data_set, goose_value.toMmsValue().value());
			}

			GoosePublisher_increaseStNum(ctx.publisher);
			GoosePublisher_publish(ctx.publisher, data_set);
			LinkedList_destroyDeep(data_set, (LinkedListValueDeleteFunction) MmsValue_delete);
		}

	}

	return sample_count;
}

GooseNode::GooseNode(const std::string &name) :
	Node(name)
{
	input.state = Input::NONE;

	input.contexts = {};
	input.mappings = {};
	input.interface_id = "lo";
	input.queue_length = 1024;

	output.state = Output::NONE;
	output.interface_id = "lo";
}

GooseNode::~GooseNode()
{
	int err __attribute__((unused));

	destroyReceiver();
	destroyPublishers();

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
	json_t *out_json = nullptr;
	ret = json_unpack_ex(json, err, 0, "{ s: o, s: o }",
		"in", &in_json,
		"out", &out_json
	);
	if (ret) return ret;

	ret = parseInput(in_json, err);
	if (ret) return ret;

	ret = parseOutput(out_json, err);
	if (ret) return ret;

	return 0;
}

int GooseNode::parseInput(json_t *json, json_error_t *err)
{
	int ret;

	json_t *subscribers_json = nullptr;
	json_t *signals_json = nullptr;
	char const *interface_id = nullptr;
	int with_timestamp = true;
	ret = json_unpack_ex(json, err, 0, "{ s: o, s: o, s?: s, s: b }",
		"subscribers", &subscribers_json,
		"signals", &signals_json,
		"interface", &interface_id,
		"with_timestamp", &with_timestamp
	);
	if (ret) return ret;

	ret = parseSubscribers(subscribers_json, err, input.contexts);
	if (ret) return ret;

	ret = parseInputSignals(signals_json, err, input.mappings);
	if (ret) return ret;

	input.interface_id = interface_id
		? std::string { interface_id }
		: std::string { "eth0" };

	input.with_timestamp = with_timestamp;

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

		auto mapping_dflt = GooseSignal::fromNameAndValue(mapping_type, signal->init);
		if (mapping_dflt.signalType() == SignalType::INVALID)
			throw RuntimeError("Invalid mms_type {}", mapping_type);

		if (signal->type != mapping_dflt.signalType())
			throw RuntimeError("Expected signal type {} for mms_type {}, but got {}",
				signalTypeToString(mapping_dflt.signalType()),
				mapping_type,
				signalTypeToString(signal->type));

		mappings.push_back(InputMapping {
			mapping_subscriber,
			mapping_index,
			mapping_dflt
		});
	}

	return 0;
}

int GooseNode::parseOutput(json_t *json, json_error_t *err)
{
	int ret;

	json_t *publishers_json = nullptr;
	json_t *signals_json = nullptr;
	char const *interface_id = nullptr;
	ret = json_unpack_ex(json, err, 0, "{ s: o, s: o, s?: s }",
		"publishers", &publishers_json,
		"signals", &signals_json,
		"interface", &interface_id
	);
	if (ret) return ret;

	ret = parsePublishers(publishers_json, err, output.contexts);
	if (ret) return ret;

	output.interface_id = interface_id
		? std::string { interface_id }
		: std::string { "eth0" };

	return 0;
}

int GooseNode::parsePublisherData(json_t *json, json_error_t *err, std::vector<OutputData> &data)
{
	int ret;
	int index;
	json_t* signal_or_value_json;

	json_array_foreach(json, index, signal_or_value_json) {

		char const *mms_type = nullptr;
		char const *signal_str = nullptr;
		json_t *value_json = nullptr;
		int integer_size = -1;
		int unsigned_size = -1;
		int bit_string_size = -1;
		int float_size = -1;
		ret = json_unpack_ex(signal_or_value_json, err, 0, "{ s:s, s?:s, s?:o, s?:i, s?:i, s?:i, s?:i }",
			"mms_type", &mms_type,
			"signal", &signal_str,
			"value", &value_json,
			"mms_integer_size", &integer_size,
			"mms_unsigned_size", &unsigned_size,
			"mms_bit_string_size", &bit_string_size,
			"mms_float_size", &float_size
		);
		if (ret) return ret;

		auto goose_type = GooseSignal::lookupMmsTypeName(mms_type).value();
		std::optional<GooseSignal::Meta> meta = std::nullopt;

		switch (goose_type->mms_type.value()) {
		case MmsType::MMS_INTEGER:
			if (integer_size != -1)
				meta = {.size = integer_size };
			break;
		case MmsType::MMS_UNSIGNED:
			if (unsigned_size != -1)
				meta = {.size = unsigned_size };
			break;
		case MmsType::MMS_BIT_STRING:
			if (bit_string_size != -1)
				meta = {.size = bit_string_size };
			break;
		case MmsType::MMS_FLOAT:
			if (float_size != -1)
				meta = {.size = float_size };
			break;
		default:
			break;
		}

		auto signal_data = SignalData {};

		if (value_json) {
			ret = signal_data.parseJson(goose_type->signal_type, value_json);
			if (ret) return ret;
		}

		auto signal = std::optional<int> {};

		if (signal_str)
			signal = out.signals->getIndexByName(signal_str);

		OutputData value = {
			.signal = signal,
			.default_value = GooseSignal { goose_type, signal_data, meta }
		};

		data.push_back(value);
	};

	return 0;
}

int GooseNode::parsePublisher(json_t *json, json_error_t *err, PublisherConfig &pc)
{
	int ret;

	char *go_id = nullptr;
	char *go_cb_ref = nullptr;
	char *data_set_ref = nullptr;
	char *dst_address_str = nullptr;
	int app_id = 0;
	int conf_rev = 0;
	int time_allowed_to_live = 0;
	json_t *data_json = nullptr;
	ret = json_unpack_ex(json, err, 0, "{ s:s, s:s, s:s, s:s, s:i, s:i, s:i, s:o }",
		"go_id", &go_id,
		"go_cb_ref", &go_cb_ref,
		"data_set_ref", &data_set_ref,
		"dst_address", &dst_address_str,
		"app_id", &app_id,
		"conf_rev", &conf_rev,
		"time_allowed_to_live", &time_allowed_to_live,
		"data", &data_json
	);
	if (ret) return ret;

	std::optional dst_address = stringToMac(dst_address_str);
	if (!dst_address)
		throw RuntimeError("Invalid dst_address");

	pc.go_id = std::string { go_id };
	pc.go_cb_ref = std::string { go_cb_ref };
	pc.data_set_ref = std::string { data_set_ref };
	pc.dst_address = *dst_address;
	pc.app_id = app_id;
	pc.conf_rev = conf_rev;
	pc.time_allowed_to_live = time_allowed_to_live;

	parsePublisherData(data_json, err, pc.data);
	if (ret) return ret;

	return 0;
}

int GooseNode::parsePublishers(json_t *json, json_error_t *err, std::vector<OutputContext> &ctx)
{
	int ret;
	int index;
	json_t* publisher_json;

	json_array_foreach(json, index, publisher_json) {
		PublisherConfig pc;

		ret = parsePublisher(publisher_json, err, pc);
		if (ret) return ret;

		ctx.push_back(OutputContext { pc });
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
	if (in.enabled)
		startReceiver();

	if (out.enabled)
		startPublishers();

	return Node::start();
}

int GooseNode::stop()
{
	int err __attribute__((unused));

	stopReceiver();
	stopPublishers();

	err = queue_signalled_close(&input.queue);

	return Node::stop();
}

static char name[] = "iec61850-goose";
static char description[] = "Subscribe to and publish GOOSE messages";
static NodePlugin<
	GooseNode,
	name,
	description,
	(int) NodeFactory::Flags::SUPPORTS_READ
	| (int) NodeFactory::Flags::SUPPORTS_WRITE
	| (int) NodeFactory::Flags::SUPPORTS_POLL,
	1
> p;
