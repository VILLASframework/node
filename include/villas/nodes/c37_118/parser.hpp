/* Parser for C37-118.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2024 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstring>
#include <optional>
#include <string>
#include <villas/nodes/c37_118/types.hpp>

namespace villas::node::c37_118::parser {
using namespace villas::node::c37_118::types;

class Parser {
public:
  Parser(Config config = {});
  void set_config(Config config);
  Config const *get_config() const;
  std::optional<Frame> deserialize(unsigned char const *buffer,
                                   std::size_t length);
  std::vector<unsigned char> serialize(Frame const &frame);

private:
  uint16_t config_num_pmu() const;
  uint16_t config_format(uint16_t pmu) const;
  uint16_t config_phnmr(uint16_t pmu) const;
  uint16_t config_annmr(uint16_t pmu) const;
  uint16_t config_dgnmr(uint16_t pmu) const;

  template <typename T> void de_copy(T *value, std::size_t count = sizeof(T)) {
    if (de_cursor + count > de_end)
      throw RuntimeError{"c37_118: broken frame"};

    std::memcpy((void *)value, de_cursor, count);
    de_cursor += count;
  }

  template <typename T>
  void se_copy(T const *value, std::size_t count = sizeof(T)) {
    auto index = se_buffer.size();
    se_buffer.insert(se_buffer.end(), count, 0);
    std::memcpy(se_buffer.data() + index, (void const *)value, count);
  }

  uint16_t deserialize_uint16_t();
  uint32_t deserialize_uint32_t();
  int16_t deserialize_int16_t();
  float deserialize_float();
  std::string deserialize_name1();
  Phasor deserialize_phasor(uint16_t pmu);
  Analog deserialize_analog(uint16_t pmu);
  Freq deserialize_freq(uint16_t pmu);
  PmuData deserialize_pmu_data(uint16_t pmu);
  PmuConfig deserialize_pmu_config_simple();
  Config deserialize_config_simple();
  Config1 deserialize_config1();
  Config2 deserialize_config2();
  Data deserialize_data();
  Header deserialize_header();
  Command deserialize_command();
  std::optional<Frame> try_deserialize_frame();

  void serialize_uint16_t(const uint16_t &value);
  void serialize_uint32_t(const uint32_t &value);
  void serialize_int16_t(const int16_t &value);
  void serialize_float(const float &value);
  void serialize_name1(const std::string &value);
  void serialize_phasor(const Phasor &value, uint16_t pmu);
  void serialize_analog(const Analog &value, uint16_t pmu);
  void serialize_freq(const Freq &value, uint16_t pmu);
  void serialize_pmu_data(const PmuData &value, uint16_t pmu);
  void serialize_pmu_config_simple(const PmuConfig &value);
  void serialize_config_simple(const Config &value);
  void serialize_config1(const Config1 &value);
  void serialize_config2(const Config2 &value);
  void serialize_data(const Data &value);
  void serialize_header(const Header &value);
  void serialize_command(const Command &value);
  void serialize_frame(const Frame &value);

  std::optional<Config> config;

  unsigned char const *de_cursor;
  unsigned char const *de_end;

  std::vector<unsigned char> se_buffer;
};

uint16_t calculate_crc(unsigned char const *frame, uint16_t size);

} // namespace villas::node::c37_118::parser
