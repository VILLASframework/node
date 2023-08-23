/** A Modbus node-type supporting RTU and TCP transports.
 *
 * The modbus communication using the libmodbus library is fairly simple.
 *
 * 1. Create a modbus_t modbus_context from the connection_settings.
 * 2. Call modbus_connect to create a connection to a server.
 * 3. Use modbus_read_registers/modbus_write_registers to read/write values.
 *
 * The complicated part is the configuration parsing, especially the mapping
 * from signals to registers. We try to group as many registers as we can
 * together to query them using a single modbus command. The general idea is:
 *
 * 1. Create a simple mapping for all signal specifications in the parse() function.
 * 2. Sort all mappings by the range of registers the need.
 * 3. Merge mappings that sit next to each other into larger groups until either ...
 *    - ... the group is larger than "max_block_size".
 *    - ... the ration of needed registers to queried registers falls below
 *      "min_block_usage". So we cap the amount of unecessary data transmitted.
 *
 * The merging process is further complicated by the possibility to map bits from
 * a register to their own signals. We don't generally want to allow mapping the
 * same register multiple times, except for the case of bit mappings.
 *
 * While a general overlap between any two mappings is considered an error, the
 * case of overlapping bit mappings is detected by hasOverlappingBitMapping and
 * handled in blockDistance and compareBlockAddress.
 *
 * - The special case in compareBlockAddress makes bit mappings of the same register
 *   reside next to each other after sorting the mappings.
 * - The special case in blockDistance makes causes the bit mappings to be grouped
 *   first, before any adjacent registers.
 *
 * @author Philipp Jungkamp <philipp.jungkamp@opal-rt.com>
 * @copyright 2023, OPAL-RT Germany GmbH
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/node_compat.hpp>
#include <villas/nodes/modbus.hpp>
#include <villas/utils.hpp>
#include <villas/sample.hpp>
#include <villas/exceptions.hpp>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>
#include <fmt/format.h>
#include <atomic>
#include <chrono>

using namespace villas;
using namespace villas::node;
using namespace villas::node::modbus;
using namespace villas::utils;

int64_t RegisterMappingSingle::IntegerToInteger::read(uint16_t const *registers) const
{
	int64_t integer = 0;
	auto ptr = word_endianess == Endianess::Big ? registers + num_registers - 1 : registers;

	for (size_t i = 0; i < num_registers; ++i) {
		integer <<= sizeof(uint16_t) * 8;

		if (byte_endianess == Endianess::Big)
			integer |= (int64_t) *ptr;
		else
			integer |= (int64_t) byteswap(*ptr);

		if (word_endianess == Endianess::Big)
			--ptr;
		else
			++ptr;
	}

	return integer;
}

void RegisterMappingSingle::IntegerToInteger::write(int64_t integer, uint16_t *registers) const
{
	auto ptr = word_endianess == Endianess::Big ? registers : registers + num_registers - 1;

	for (size_t i = 0; i < num_registers; ++i) {
		if (byte_endianess == Endianess::Big)
			*ptr = (uint16_t) integer;
		else
			*ptr = byteswap((uint16_t) integer);

		if (word_endianess == Endianess::Big)
			++ptr;
		else
			--ptr;

		integer >>= sizeof(uint16_t) * 8;
	}
}

double RegisterMappingSingle::IntegerToFloat::read(uint16_t const *registers) const
{
	int64_t integer = integer_conversion.read(registers);

	return integer * scale + offset;
}

void RegisterMappingSingle::IntegerToFloat::write(double d, uint16_t *registers) const
{
	int64_t integer = (d - offset) / scale;

	integer_conversion.write(integer, registers);
}

double RegisterMappingSingle::FloatToFloat::read(uint16_t const *registers) const
{
	static_assert(sizeof(float) == sizeof(uint32_t));

	auto const conversion = IntegerToInteger {
		.word_endianess = word_endianess,
		.byte_endianess = byte_endianess,
		.num_registers = 2,
	};

	union {
		uint32_t i;
		float f;
	} value;

	value.i = (uint32_t) conversion.read(registers);

	return value.f * scale + offset;
}

void RegisterMappingSingle::FloatToFloat::write(double d, uint16_t *registers) const
{
	static_assert(sizeof(float) == sizeof(uint32_t));

	auto const conversion = IntegerToInteger {
		.word_endianess = word_endianess,
		.byte_endianess = byte_endianess,
		.num_registers = 2,
	};

	union {
		uint32_t i;
		float f;
	} value;

	value.f = (d - offset) / scale;

	conversion.write((int64_t) value.i, registers);
}

bool RegisterMappingSingle::BitToBool::read(uint16_t reg) const
{
	return (reg >> bit) & 1;
}

RegisterMappingSingle::RegisterMappingSingle(unsigned int signal_index, modbus_addr_t address) :
	conversion(IntegerToInteger {
		.word_endianess = Endianess::Big,
		.byte_endianess = Endianess::Big,
		.num_registers = 1,
	}),
	signal_index(signal_index),
	address(address)
{ }

SignalData RegisterMappingSingle::read(uint16_t const *registers, modbus_addr_t length) const
{
	SignalData data;

	if (num_registers() != length)
		throw RuntimeError { "reading from invalid register range" };

	if (auto i2i = std::get_if<IntegerToInteger>(&conversion))
		data.i = i2i->read(registers);
	else if (auto i2f = std::get_if<IntegerToFloat>(&conversion))
		data.f = i2f->read(registers);
	else if (auto f2f = std::get_if<FloatToFloat>(&conversion))
		data.f = f2f->read(registers);
	else if (auto b2b = std::get_if<BitToBool>(&conversion))
		data.b = b2b->read(*registers);
	else
		throw RuntimeError { "read unsupported" };

	return data;
}

void RegisterMappingSingle::write(SignalData data, uint16_t *registers, modbus_addr_t length) const
{
	if (num_registers() != length)
		throw RuntimeError { "writing to invalid register range" };

	if (auto i2i = std::get_if<IntegerToInteger>(&conversion))
		i2i->write(data.i, registers);
	else if (auto i2f = std::get_if<IntegerToFloat>(&conversion))
		i2f->write(data.f, registers);
	else if (auto f2f = std::get_if<FloatToFloat>(&conversion))
		f2f->write(data.f, registers);
	else
		throw RuntimeError { "write unsupported" };
}

modbus_addr_t RegisterMappingSingle::num_registers() const
{
	if (auto i2i = std::get_if<IntegerToInteger>(&conversion))
		return i2i->num_registers;

	if (auto i2f = std::get_if<IntegerToFloat>(&conversion))
		return i2f->integer_conversion.num_registers;

	if (std::holds_alternative<FloatToFloat>(conversion))
		return 2;

	if (std::holds_alternative<BitToBool>(conversion))
		return 1;

	throw RuntimeError { "unreachable" };
}

uint16_t modbus::byteswap(uint16_t i)
{
	uint8_t low = (i & 0x00FF);
	uint8_t high = (i & 0xFF00) >> 8;
	return (low << 8) | high;
}

modbus_addr_t modbus::blockBegin(RegisterMappingSingle const &single)
{
	return single.address;
}

modbus_addr_t modbus::blockBegin(RegisterMappingBlock const &block)
{
	assert(!block.empty());
	return blockBegin(block.front());
}

modbus_addr_t modbus::blockBegin(RegisterMapping const &mapping)
{
	return std::visit([](auto &v){
		return blockBegin(v);
	}, mapping);
}

modbus_addr_t modbus::blockEnd(RegisterMappingSingle const &single)
{
	return single.address + single.num_registers();
}

modbus_addr_t modbus::blockEnd(RegisterMappingBlock const &block)
{
	assert(!block.empty());
	return blockEnd(block.back());
}

modbus_addr_t modbus::blockEnd(RegisterMapping const &mapping)
{
	return std::visit([](auto &v){
		return blockEnd(v);
	}, mapping);
}


modbus_addr_t modbus::mappedRegisters(RegisterMappingSingle const &single)
{
	return single.num_registers();
}

modbus_addr_t modbus::mappedRegisters(RegisterMappingBlock const &block)
{
	auto mapped = 0;

	modbus_addr_t last_address = -1;
	for (auto &single : block) {
		if (single.address != last_address)
			mapped += single.num_registers();

		last_address = single.address;
	}

	return mapped;
}

modbus_addr_t modbus::mappedRegisters(RegisterMapping const &mapping)
{
	return std::visit([](auto &v){
		return mappedRegisters(v);
	}, mapping);
}

modbus_addrdiff_t modbus::blockDistance(RegisterMapping const &lhs, RegisterMapping const &rhs)
{
	if (blockBegin(rhs) >= blockEnd(lhs))
		return (modbus_addrdiff_t) blockBegin(rhs) - blockEnd(lhs);

	if (blockBegin(lhs) >= blockEnd(rhs))
		return (modbus_addrdiff_t) blockBegin(lhs) - blockEnd(rhs);

	if (hasOverlappingBitMapping(lhs, rhs))
		return -1;

	throw RuntimeError { "overlapping mappings" };
}

bool modbus::hasOverlappingBitMapping(RegisterMapping const &lhs, RegisterMapping const &rhs)
{
	// Only check if there is exactly 1 register of overlap.
	if (blockEnd(lhs) - blockBegin(rhs) != 1 && blockEnd(rhs) - blockBegin(lhs) != 1)
		return false;

	// Assume that lhs is at a lower address than rhs.
	if (blockBegin(rhs) < blockBegin(lhs) || blockEnd(rhs) < blockEnd(lhs))
		return hasOverlappingBitMapping(rhs, lhs);

	// Get the last mapping from the lhs block.
	RegisterMappingSingle const *lhs_back = nullptr;
	if (auto single = std::get_if<RegisterMappingSingle>(&lhs))
		lhs_back = single;
	else if (auto block = std::get_if<RegisterMappingBlock>(&lhs))
		lhs_back = &block->back();
	else
		return false;

	// We are only interested in bit mappings.
	if (!std::holds_alternative<RegisterMappingSingle::BitToBool>(lhs_back->conversion))
		return false;

	// Get the first mapping from the rhs block.
	RegisterMappingSingle const *rhs_front = nullptr;
	if (auto single = std::get_if<RegisterMappingSingle>(&rhs))
		rhs_front = single;
	else if (auto block = std::get_if<RegisterMappingBlock>(&rhs))
		rhs_front = &block->front();
	else
		return false;

	// We are only interested in bit mappings.
	if (!std::holds_alternative<RegisterMappingSingle::BitToBool>(rhs_front->conversion))
		return false;

	// The last register of lhs and the first register of rhs overlap and are both bit mappings.
	return true;
}

bool modbus::compareBlockAddress(RegisterMapping const &lhs, RegisterMapping const &rhs)
{
	if (blockBegin(rhs) >= blockEnd(lhs))
		return true;

	if (blockBegin(lhs) >= blockEnd(rhs))
		return false;

	if (hasOverlappingBitMapping(lhs, rhs))
		return false;

	throw RuntimeError { "overlapping mappings" };
}

bool ModbusNode::isReconnecting()
{
	return reconnecting.load();
}

void ModbusNode::reconnect()
{
	if (reconnecting.exchange(true))
		return;

	logger->error("No connection to the Modbus server. Reconnecting...");

	std::thread([this](){
		auto start = std::chrono::steady_clock::now();

		if (modbus_connect(modbus_context) == -1) {
			logger->error("reconnect failure: ", modbus_strerror(errno));
			std::this_thread::sleep_until(start + std::chrono::duration<double>(reconnect_interval));
		}

		reconnecting.store(false);
	}).detach();
}

void ModbusNode::mergeMappingInplace(RegisterMapping &lhs, RegisterMappingBlock const &rhs)
{
	if (auto lhs_single = std::get_if<RegisterMappingSingle>(&lhs))
		lhs = RegisterMappingBlock { *lhs_single };

	auto &block = std::get<RegisterMappingBlock>(lhs);
	block.reserve(blockEnd(rhs) - blockBegin(lhs));
	std::copy(std::begin(rhs), std::end(rhs), std::back_inserter(block));
}

void ModbusNode::mergeMappingInplace(RegisterMapping &lhs, RegisterMappingSingle const &rhs)
{
	if (auto lhs_single = std::get_if<RegisterMappingSingle>(&lhs))
		lhs = RegisterMappingBlock { *lhs_single };

	auto &block = std::get<RegisterMappingBlock>(lhs);
	block.push_back(rhs);
}

bool ModbusNode::tryMergeMappingInplace(RegisterMapping &lhs, RegisterMapping const &rhs)
{
	auto block_size = blockEnd(rhs) - blockBegin(lhs);

	if (block_size >= max_block_size)
		return false;

	auto block_usage = (mappedRegisters(lhs) + mappedRegisters(rhs)) / (float) block_size;

	if (block_usage < min_block_usage)
		return false;

	std::visit([&lhs](auto const &rhs){
		mergeMappingInplace(lhs, rhs);
	}, rhs);

	return true;
}

void ModbusNode::mergeMappings(std::vector<RegisterMapping> &mappings, modbus_addrdiff_t max_block_distance)
{
	if (std::size(mappings) < 2)
		return;

	// Sort all mappings by their block address.
	std::sort(std::begin(mappings), std::end(mappings), compareBlockAddress);

	// Calculate the distances. (number of unused registers inbetween mappings)
	auto distances = std::vector<int>();
	distances.reserve(std::size(mappings));
	for (size_t i = 1; i < std::size(mappings); i++)
		distances.push_back(blockDistance(mappings[i-1], mappings[i]));

	for (;;) {
		// Try to group the mappings closest to each other first.
		auto min_distance = std::min_element(std::begin(distances), std::end(distances));

		// The closest distance is too far to merge, abort the merging process.
		if (min_distance == std::end(distances) || *min_distance >= max_block_distance)
			break;

		// Find the mappings to the left and right of the minimum distance.
		auto i = std::distance(std::begin(distances), min_distance);
		auto left_mapping = std::next(std::begin(mappings), i);
		auto right_mapping = std::next(std::begin(mappings), i+1);

		if (tryMergeMappingInplace(*left_mapping, *right_mapping)) {
			// Remove the right mapping and the distance
			// if it could be merged into the left mapping.
			mappings.erase(right_mapping);
			distances.erase(min_distance);
		} else {
			// Set the distance to a value, so that it won't be retried.
			*min_distance = max_block_distance;
		}
	}
}

int ModbusNode::readBlock(RegisterMapping const &mapping, SignalData *data, size_t size)
{
	if (isReconnecting())
		return -1;

	auto address = blockBegin(mapping);
	auto block_size = blockEnd(mapping) - address;

	read_buffer.resize(block_size);

	if (modbus_read_registers(modbus_context, address, block_size, read_buffer.data()) == -1) {
		logger->error("read registers failure: ", modbus_strerror(errno));

		reconnect();

		return -1;
	}

	return readMapping(mapping, read_buffer.data(), read_buffer.size(), data, size);
}

int ModbusNode::readMapping(RegisterMapping const &mapping, uint16_t const *registers, modbus_addr_t num_registers, SignalData *signals, unsigned int num_signals)
{
	return std::visit([this, registers, num_registers, signals, num_signals](auto mapping){
		return readMapping(mapping, registers, num_registers, signals, num_signals);
	}, mapping);
}

int ModbusNode::readMapping(RegisterMappingSingle const &single, uint16_t const *registers, modbus_addr_t num_registers, SignalData *signals, unsigned int num_signals)
{
	auto signal_data = single.read(registers, num_registers);

	assert(single.signal_index < num_signals);
	signals[single.signal_index] = signal_data;

	return 0;
}

int ModbusNode::readMapping(RegisterMappingBlock const &block, uint16_t const *registers, modbus_addr_t num_registers, SignalData *signals, unsigned int num_signals)
{
	auto begin_block = blockBegin(block);

	for (auto &single : block) {
		auto begin_single = blockBegin(single);
		auto end_single = blockEnd(single);

		assert(end_single - begin_block <= num_registers);
		if (auto ret = readMapping(single, &registers[begin_single - begin_block], end_single - begin_single, signals, num_signals))
			return ret;
	}

	return 0;
}

int ModbusNode::_read(struct Sample *smps[], unsigned cnt)
{
	read_task.wait();

	for (unsigned int i = 0; i < cnt; ++i) {
		auto smp = smps[i];
		smp->flags |= (int) SampleFlags::HAS_DATA;

		for (auto &mapping : in_mappings) {
			smp->length = num_in_signals;
			assert(smp->length <= smp->capacity);
			if (auto ret = readBlock(mapping, smp->data, smp->length))
				return ret;
		}
	}

	return cnt;
}

int ModbusNode::writeBlock(RegisterMapping const &mapping, SignalData const *data, size_t size)
{
	if (isReconnecting())
		return -1;

	auto address = blockBegin(mapping);
	auto block_size = blockEnd(mapping) - address;

	write_buffer.resize(block_size);

	if (auto ret = writeMapping(mapping, write_buffer.data(), write_buffer.size(), data, size))
		return ret;

	if (modbus_write_registers(modbus_context, address, block_size, write_buffer.data()) == -1) {
		logger->error("write registers failure: ", modbus_strerror(errno));

		reconnect();

		return -1;
	}

	return 0;
}

int ModbusNode::writeMapping(RegisterMapping const &mapping, uint16_t *registers, modbus_addr_t num_registers, SignalData const *signals, unsigned int num_signals)
{
	return std::visit([this, registers, num_registers, signals, num_signals](auto mapping){
		return writeMapping(mapping, registers, num_registers, signals, num_signals);
	}, mapping);
}

int ModbusNode::writeMapping(RegisterMappingSingle const &single, uint16_t *registers, modbus_addr_t num_registers, SignalData const *signals, unsigned int num_signals)
{
	assert(single.signal_index < num_signals);
	single.write(signals[single.signal_index], registers, num_registers);

	return 0;
}

int ModbusNode::writeMapping(RegisterMappingBlock const &block, uint16_t *registers, modbus_addr_t num_registers, SignalData const *signals, unsigned int num_signals)
{
	auto begin_block = blockBegin(block);

	for (auto &single : block) {
		auto begin_single = blockBegin(single);
		auto end_single = blockEnd(single);

		assert(end_single - begin_block <= num_registers);
		if (auto ret = writeMapping(single, &registers[begin_single - begin_block], end_single - begin_single, signals, num_signals))
			return ret;
	}

	return 0;
}

int ModbusNode::_write(struct Sample *smps[], unsigned cnt)
{
	for (unsigned int i = 0; i < cnt; ++i) {
		auto smp = smps[i];

		for (auto &mapping : out_mappings) {
			assert(smp->length == num_out_signals);
			if (auto ret = writeBlock(mapping, smp->data, smp->length))
				return ret;
		}
	}

	return cnt;
}

ModbusNode::ModbusNode(const uuid_t &id, const std::string &name) :
	Node(id, name),
	max_block_size(32),
	min_block_usage(0.25),
	connection_settings(),
	rate(-1),
	response_timeout(1),
	in_mappings{},
	num_in_signals(0),
	out_mappings{},
	num_out_signals(0),
	reconnect_interval(10),
	read_buffer{},
	write_buffer{},
	modbus_context(nullptr),
	read_task(),
	reconnecting(false)
{ }

ModbusNode::~ModbusNode()
{
	if (modbus_context)
		modbus_free(modbus_context);
}

int ModbusNode::prepare()
{
	mergeMappings(in_mappings, max_block_size - 2);
	mergeMappings(out_mappings, 0);

	if (in.enabled) {
		read_task.setRate(rate);

		logger->info("Making {} Modbus calls for each read", in_mappings.size());
	}

	if (out.enabled)
		logger->info("Making {} Modbus calls for each write", out_mappings.size());

	assert(!std::holds_alternative<std::monostate>(connection_settings));

	if (auto tcp = std::get_if<Tcp>(&connection_settings)) {
		modbus_context = modbus_new_tcp(tcp->remote.c_str(), tcp->port);

		if (tcp->unit)
			modbus_set_slave(modbus_context, *tcp->unit);
	}

	if (auto rtu = std::get_if<Rtu>(&connection_settings)) {
		modbus_context = modbus_new_rtu(
			rtu->device.c_str(),
			rtu->baudrate,
			static_cast<char>(rtu->parity),
			rtu->data_bits,
			rtu->stop_bits);

		modbus_set_slave(modbus_context, rtu->unit);
	}

	auto response_timeout_secs = (uint32_t) response_timeout;
	auto response_timeout_usecs = (uint32_t) (response_timeout - (double) response_timeout_secs);
	modbus_set_response_timeout(modbus_context, response_timeout_secs, response_timeout_usecs);

	return Node::prepare();
}

Endianess modbus::parseEndianess(char const *str)
{
	if (!strcmp(str, "little"))
		return Endianess::Little;

	if (!strcmp(str, "big"))
		return Endianess::Big;

	throw RuntimeError { "invalid endianess" };
}

Parity modbus::parseParity(char const *str)
{
	if (!strcmp(str, "none"))
		return Parity::None;

	if (!strcmp(str, "even"))
		return Parity::Even;

	if (!strcmp(str, "odd"))
		return Parity::Odd;

	throw RuntimeError { "invalid parity" };
}

Rtu Rtu::parse(json_t* json)
{
	char const *device = nullptr;
	char const *parity_str = nullptr;
	int baudrate = -1;
	int data_bits = -1;
	int stop_bits = -1;

	json_error_t err;
	int ret = json_unpack_ex(json, &err, 0, "{ s: s, s: s, s: i, s: i, s: i }",
		"device", &device,
		"parity", &parity_str,
		"baudrate", &baudrate,
		"data_bits", &data_bits,
		"stop_bits", &stop_bits
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-modbus-rtu");

	Parity parity = parseParity(parity_str);

	return Rtu { device, parity, baudrate, data_bits, stop_bits };
}

Tcp Tcp::parse(json_t* json)
{
	char const *remote = nullptr;
	int port = 502;
	int unit_int = -1;

	json_error_t err;
	int ret = json_unpack_ex(json, &err, 0, "{ s: s, s?: i, s?: i }",
		"remote", &remote,
		"port", &port,
		"unit", &unit_int
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-modbus-tcp");

	std::optional unit = unit_int >= 0 ? std::optional(unit_int) : std::nullopt;

	return Tcp {
		.remote = remote,
		.port = (uint16_t) port,
		.unit = unit,
	};
}

RegisterMappingSingle RegisterMappingSingle::parse(unsigned int index, Signal::Ptr signal, json_t *json)
{
	int address = -1;
	int bit = -1;
	int integer_registers = -1;
	char const *word_endianess_str = nullptr;
	char const *byte_endianess_str = nullptr;
	double offset = 0.0;
	double scale = 1.0;

	json_error_t err;
	int ret = json_unpack_ex(json, &err, 0, "{ s: i, s?: i, s?: i, s?: s, s?: s, s?: F, s?: F }",
		"address", &address,
		"bit", &bit,
		"integer_registers", &integer_registers,
		"word_endianess", &word_endianess_str,
		"byte_endianess", &byte_endianess_str,
		"offset", &offset,
		"scale", &scale
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-modbus-signal");

	if (integer_registers != -1 && (integer_registers <= 0 || (size_t) integer_registers > MAX_REGISTERS))
		throw RuntimeError { "unsupported register block size" };

	Endianess word_endianess = Endianess::Big;
	if (word_endianess_str)
		word_endianess = parseEndianess(word_endianess_str);

	Endianess byte_endianess = Endianess::Big;
	if (byte_endianess_str)
		byte_endianess = parseEndianess(byte_endianess_str);

	auto mapping = RegisterMappingSingle { index, (modbus_addr_t) address };
	if (signal->type == SignalType::FLOAT) {
		if (integer_registers == -1) {
			mapping.conversion = FloatToFloat {
				.word_endianess = word_endianess,
				.byte_endianess = byte_endianess,
				.offset = offset,
				.scale = scale,
			};
		} else {
			auto integer_conversion = IntegerToInteger {
				.word_endianess = word_endianess,
				.byte_endianess = byte_endianess,
				.num_registers = (modbus_addr_t) integer_registers,
			};

			mapping.conversion = IntegerToFloat {
				.integer_conversion = integer_conversion,
				.offset = offset,
				.scale = scale,
			};
		}

		return mapping;
	} else if (signal->type == SignalType::INTEGER) {
		if (integer_registers == -1)
			integer_registers = 1;

		mapping.conversion = IntegerToInteger {
			.word_endianess = word_endianess,
			.byte_endianess = byte_endianess,
			.num_registers = (modbus_addr_t) integer_registers,
		};
	} else if (signal->type == SignalType::BOOLEAN) {
		if (bit < 0 || bit > 15)
			throw RuntimeError { "mappings from bit to bool must be in the range 0 to 16" };

		mapping.conversion = BitToBool {
			.bit = (uint8_t) bit,
		};
	} else {
		throw RuntimeError { "unsupported signal type" };
	}

	return mapping;
}

unsigned int ModbusNode::parseMappings(std::vector<RegisterMapping> &mappings, json_t *json)
{
	assert(json_is_array(json));

	size_t i;
	json_t *signal_json;
	auto signals = getInputSignals(false);

	json_array_foreach (json, i, signal_json) {
		auto signal = signals->getByIndex(i);

		mappings.push_back(RegisterMappingSingle::parse(i, signal, signal_json));
	}

	return json_array_size(json);
}

int ModbusNode::parse(json_t *json)
{
	if (auto ret = Node::parse(json))
		return ret;

	json_error_t err;
	char const * transport = nullptr;
	json_t *in_json = nullptr;
	json_t *out_json = nullptr;

	if (json_unpack_ex(json, &err, 0, "{ s: s, s?: F, s?: F, s?: i, s?: i, s?: F, s?: o, s?: o }",
		"transport", &transport,
		"response_timeout", &response_timeout,
		"reconnect_interval", &reconnect_interval,
		"min_block_usage", &min_block_usage,
		"max_block_size", &max_block_size,
		"rate", &rate,
		"in", &in_json,
		"out", &out_json
	))
		throw ConfigError(json, err, "node-config-node-modbus");

	if (in.enabled && rate < 0)
		throw RuntimeError { "missing polling rate for Modbus reads" };

	if (!strcmp(transport, "rtu"))
		connection_settings = Rtu::parse(json);
	else if (!strcmp(transport, "tcp"))
		connection_settings = Tcp::parse(json);
	else
		throw ConfigError(json, err, "node-config-node-modbus-transport");

	json_t *signals_json;

	if (in_json && (signals_json = json_object_get(in_json, "signals")))
		num_in_signals = parseMappings(in_mappings, signals_json);

	if (out_json && (signals_json = json_object_get(out_json, "signals")))
		num_out_signals = parseMappings(out_mappings, signals_json);

	return 0;
}

int ModbusNode::check()
{
	return Node::check();
}

int ModbusNode::start()
{
	if (modbus_connect(modbus_context) == -1)
		throw RuntimeError { "connection failure: {}", modbus_strerror(errno) };

	return Node::start();
}

int ModbusNode::stop()
{
	modbus_close(modbus_context);

	return Node::stop();
}

std::vector<int> ModbusNode::getPollFDs()
{
	return { read_task.getFD() };
}

const std::string & ModbusNode::getDetails()
{
	details = fmt::format("");
	return details;
}

static char name[] = "modbus";
static char description[] = "read and write Modbus registers as a client";
static NodePlugin<ModbusNode, name, description, (int) NodeFactory::Flags::SUPPORTS_READ | (int) NodeFactory::Flags::SUPPORTS_WRITE | (int) NodeFactory::Flags::SUPPORTS_POLL> p;
