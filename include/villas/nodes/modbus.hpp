/* A Modbus node-type supporting RTU and TCP based transports.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@opal-rt.com>
 * SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <algorithm>
#include <numeric>
#include <optional>
#include <variant>
#include <vector>

#include <modbus/modbus.h>
#include <stdint.h>

#include <villas/exceptions.hpp>
#include <villas/node.hpp>
#include <villas/node/config.hpp>
#include <villas/sample.hpp>
#include <villas/task.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {
namespace modbus {

using modbus_addr_t = uint16_t;
using modbus_addrdiff_t = int32_t;

enum class Parity : char {
  None = 'N',
  Even = 'E',
  Odd = 'O',
};

enum class Endianess : char {
  Big,
  Little,
};

// The settings for an RTU modbus connection.
struct Rtu {
  std::string device;
  Parity parity;
  int baudrate;
  int data_bits;
  int stop_bits;
  unsigned char unit;

  static Rtu parse(json_t *json);
};

// The settings for an TCP MODBUS connection.
struct Tcp {
  std::string remote;
  uint16_t port;
  std::optional<unsigned char> unit;

  static Tcp parse(json_t *json);
};

// Forward declarations
class RegisterMappingSingle;

// A merged block of mappings.
using RegisterMappingBlock = std::vector<RegisterMappingSingle>;

// Either a single mapping or a merged block of mappings.
using RegisterMapping =
    std::variant<RegisterMappingSingle, RegisterMappingBlock>;

// Swap the two bytes of a 16 bit integer.
uint16_t byteswap(uint16_t i);

// The start of a single register mapping.
modbus_addr_t blockBegin(RegisterMappingSingle const &single);

// The start of a block of register mappings.
modbus_addr_t blockBegin(RegisterMappingBlock const &block);

// The start of either a single or a block of register mappings.
modbus_addr_t blockBegin(RegisterMapping const &mapping);

// The end of a single register mapping.
modbus_addr_t blockEnd(RegisterMappingSingle const &single);

// The end of a block of register mappings.
modbus_addr_t blockEnd(RegisterMappingBlock const &block);

// The end of either a single or a block of register mappings.
modbus_addr_t blockEnd(RegisterMapping const &mapping);

// The number of mapped registers in a single register mapping.
modbus_addr_t mappedRegisters(RegisterMappingSingle const &single);

// The number of mapped registers in a block of register mappings.
modbus_addr_t mappedRegisters(RegisterMappingBlock const &block);

// The number of mapped registers in either a single or a block of register mappings.
modbus_addr_t mappedRegisters(RegisterMapping const &mapping);

// The distance between two blocks.
modbus_addrdiff_t blockDistance(RegisterMapping const &lhs,
                                RegisterMapping const &rhs);

// Whether there are overlapping bit mappings between lhs and rhs.
bool hasOverlappingBitMapping(RegisterMapping const &lhs,
                              RegisterMapping const &rhs);

// The compare the addresses of two mappings.
bool compareBlockAddress(RegisterMapping const &lhs,
                         RegisterMapping const &rhs);

// Parse an Endianess from a null terminated string.
Endianess parseEndianess(char const *str);

// Parse an Parity from a null terminated string.
Parity parseParity(char const *str);

// The mapping from a register to a signal.
class RegisterMappingSingle {
public:
  inline static constexpr size_t MAX_REGISTERS =
      sizeof(int64_t) / sizeof(int16_t);

  struct IntegerToInteger {
    Endianess word_endianess;
    Endianess byte_endianess;
    modbus_addr_t num_registers;

    int64_t read(uint16_t const *registers) const;
    void write(int64_t i, uint16_t *registers) const;
  };

  struct IntegerToFloat {
    IntegerToInteger integer_conversion;
    double offset;
    double scale;

    double read(uint16_t const *registers) const;
    void write(double d, uint16_t *registers) const;
  };

  struct FloatToFloat {
    Endianess word_endianess;
    Endianess byte_endianess;
    double offset;
    double scale;

    double read(uint16_t const *registers) const;
    void write(double d, uint16_t *registers) const;
  };

  struct BitToBool {
    uint8_t bit;

    bool read(uint16_t reg) const;
  };

  // Conversion rule for registers.
  //
  // - IntegerToInteger means merging multiple registers.
  // - FloatToFloat converts from IEEE float to float.
  // - IntegerToFloat converts registers to integer, casts to float
  //   and then applies offset and scale to produce a float.
  // - BitToBool takes a single bit from a registers and reports it as a boolean.
  std::variant<IntegerToInteger, IntegerToFloat, FloatToFloat, BitToBool>
      conversion;

  RegisterMappingSingle(unsigned int signal_index, modbus_addr_t address);

  unsigned int signal_index;
  modbus_addr_t address;

  static RegisterMappingSingle parse(unsigned int index, Signal::Ptr signal,
                                     json_t *json);

  SignalData read(uint16_t const *registers, modbus_addr_t length) const;
  void write(SignalData data, uint16_t *registers, modbus_addr_t length) const;

  modbus_addr_t num_registers() const;
};

class ModbusNode final : public Node {
private:
  // The maximum size of a RegisterMappingBlock created during mergeMappings.
  // The size of a block here is defined as the difference between blockBegin and blockEnd.
  modbus_addr_t max_block_size;

  // The minimum block usage of a RegisterMappingBlock created during mergeMappings.
  // The usage of a block is defined as the ration of registers used in mappings to the size of the block.
  // The size of a block here is defined as the difference between blockBegin and blockEnd.
  float min_block_usage;

  // The type of connection settings used to initialize the modbus_context.
  std::variant<std::monostate, Tcp, Rtu> connection_settings;

  // The rate used for periodically querying the modbus device registers.
  double rate;

  // The timeout in seconds when waiting for a response from a modbus slave/server.
  double response_timeout;

  // Mappings used to create the input signals from the read registers.
  std::vector<RegisterMapping> in_mappings;

  // Number of in signals.
  unsigned int num_in_signals;

  // Mappings used to create the input signals from the read registers.
  std::vector<RegisterMapping> out_mappings;

  // Number of out signals.
  unsigned int num_out_signals;

  // The interval in seconds for trying to reconnect on connection loss.
  double reconnect_interval;

  std::vector<uint16_t> read_buffer;
  std::vector<uint16_t> write_buffer;
  modbus_t *modbus_context;
  Task read_task;
  std::atomic<bool> reconnecting;

  bool isReconnecting();
  void reconnect();

  static void mergeMappingInplace(RegisterMapping &lhs,
                                  RegisterMappingBlock const &rhs);

  static void mergeMappingInplace(RegisterMapping &lhs,
                                  RegisterMappingSingle const &rhs);

  bool tryMergeMappingInplace(RegisterMapping &lhs, RegisterMapping const &rhs);

  void mergeMappings(std::vector<RegisterMapping> &mappings,
                     modbus_addrdiff_t max_block_distance);

  unsigned int parseMappings(std::vector<RegisterMapping> &mappings,
                             json_t *json);

  int readMapping(RegisterMappingSingle const &mapping,
                  uint16_t const *registers, modbus_addr_t num_registers,
                  SignalData *signals, unsigned int num_signals);

  int readMapping(RegisterMappingBlock const &mapping,
                  uint16_t const *registers, modbus_addr_t num_registers,
                  SignalData *signals, unsigned int num_signals);

  int readMapping(RegisterMapping const &mapping, uint16_t const *registers,
                  modbus_addr_t num_registers, SignalData *signals,
                  unsigned int num_signals);

  int readBlock(RegisterMapping const &mapping, SignalData *signals,
                size_t num_signals);

  int writeMapping(RegisterMappingSingle const &mapping, uint16_t *registers,
                   modbus_addr_t num_registers, SignalData const *signals,
                   unsigned int num_signals);
  int writeMapping(RegisterMappingBlock const &mapping, uint16_t *registers,
                   modbus_addr_t num_registers, SignalData const *signals,
                   unsigned int num_signals);
  int writeMapping(RegisterMapping const &mapping, uint16_t *registers,
                   modbus_addr_t num_registers, SignalData const *signals,
                   unsigned int num_signals);
  int writeBlock(RegisterMapping const &mapping, SignalData const *signals,
                 size_t num_signals);

  int _read(struct Sample *smps[], unsigned int cnt) override;
  int _write(struct Sample *smps[], unsigned int cnt) override;

public:
  ModbusNode(const uuid_t &id = {}, const std::string &name = "");

  virtual ~ModbusNode();

  int prepare() override;

  int parse(json_t *json) override;

  int start() override;

  int stop() override;

  std::vector<int> getPollFDs() override;

  std::vector<int> getNetemFDs() override;

  const std::string &getDetails() override;
};

} // namespace modbus
} // namespace node
} // namespace villas
