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
  std::optional<Frame> deserialize(const unsigned char *buffer,
                                   std::size_t length,
                                   const Config *config);

  std::vector<unsigned char> serialize(const Frame &frame,
                                       const Config *config);

private:
  template <typename T> void de_copy(T *value, std::size_t count = sizeof(T)) {
    if (de_cursor + count > de_end)
      throw RuntimeError{"c37_118: broken frame"};

    std::memcpy((void *)value, de_cursor, count);
    de_cursor += count;
  }

  template <typename T>
  void se_copy(const T *value, std::size_t count = sizeof(T)) {
    auto index = se_buffer.size();
    se_buffer.insert(se_buffer.end(), count, 0);
    std::memcpy(se_buffer.data() + index, (const void *)value, count);
  }

  uint16_t deserialize_uint16_t();
  uint32_t deserialize_uint32_t();
  int16_t deserialize_int16_t();
  float deserialize_float();
  std::string deserialize_name1();
  std::complex<float> deserialize_phasor(uint16_t format, const PhasorInfo &info);
  float deserialize_freq(const PmuConfig &pmu);
  float deserialize_dfreq(uint16_t format);
  float deserialize_analog(uint16_t format, const AnalogInfo &aninfo);
  uint16_t deserialize_digital(const DigitalInfo &dginfo);
  PmuData deserialize_pmu_data(const PmuConfig &pmu_config);
  PmuConfig deserialize_pmu_config_simple();
  Config deserialize_config_simple();
  Config1 deserialize_config1();
  Config2 deserialize_config2();
  Data deserialize_data(const Config &config);
  Header deserialize_header();
  Command deserialize_command();
  std::optional<Frame> try_deserialize_frame(const Config *config);

  void serialize_uint16_t(const uint16_t &value);
  void serialize_uint32_t(const uint32_t &value);
  void serialize_int16_t(const int16_t &value);
  void serialize_float(const float &value);
  void serialize_name1(const std::string &value);
  void serialize_phasor(const std::complex<float> &value, uint16_t format, const PhasorInfo &phinfo);
  void serialize_freq(const float &value, const PmuConfig &pmu);
  void serialize_dfreq(const float &value, uint16_t format);
  void serialize_analog(const float &value, uint16_t format, const AnalogInfo &aninfo);
  void serialize_digital(const uint16_t &value, const DigitalInfo &dginfo);
  void serialize_pmu_data(const PmuData &value, const PmuConfig &pmu_config);
  void serialize_pmu_config_simple(const PmuConfig &value);
  void serialize_config_simple(const Config &value);
  void serialize_config1(const Config1 &value);
  void serialize_config2(const Config2 &value);
  void serialize_data(const Data &value, const Config &config);
  void serialize_header(const Header &value);
  void serialize_command(const Command &value);
  void serialize_frame(const Frame &value, const Config *config);

  const unsigned char *de_cursor;
  const unsigned char *de_end;
  std::vector<unsigned char> se_buffer;
};

uint16_t calculate_crc(const unsigned char *frame, uint16_t size);

} // namespace villas::node::c37_118::parser
