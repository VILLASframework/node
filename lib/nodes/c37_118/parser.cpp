/* Parser for C37-118.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2024 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>
#include <villas/exceptions.hpp>
#include <villas/nodes/c37_118/parser.hpp>
#include <villas/utils.hpp>

using namespace villas::node::c37_118;
using namespace villas::node::c37_118::parser;

// Sample CRC routine taken from IEEE Std C37.118.2-2011 Annex B
//
// TODO: Is this code license compatible to Apache 2.0?
uint16_t parser::calculate_crc(unsigned char const *frame, uint16_t size) {
  uint16_t crc = 0xFFFF;
  uint16_t temp;
  uint16_t quick;

  for (int i = 0; i < size; i++) {
    temp = (crc >> 8) ^ (uint16_t)frame[i];
    crc <<= 8;
    quick = temp ^ (temp >> 4);
    crc ^= quick;
    quick <<= 5;
    crc ^= quick;
    quick <<= 7;
    crc ^= quick;
  }

  return crc;
}

Parser::Parser(Config config) : config{std::move(config)} {}

void Parser::set_config(Config config) { this->config = config; }

std::optional<Frame> Parser::deserialize(unsigned char const *buffer,
                                         std::size_t length) {
  de_cursor = buffer;
  de_end = buffer + length;
  return try_deserialize_frame();
}

std::vector<unsigned char> Parser::serialize(Frame const &frame) {
  se_buffer.clear();
  serialize_frame(frame);
  return se_buffer;
}

uint16_t Parser::config_num_pmu() const { return config.value().pmus.size(); }

uint16_t Parser::config_format(uint16_t pmu) const {
  return config.value().pmus[pmu].format;
}

uint16_t Parser::config_phnmr(uint16_t pmu) const {
  return config.value().pmus[pmu].phinfo.size();
}

uint16_t Parser::config_annmr(uint16_t pmu) const {
  return config.value().pmus[pmu].aninfo.size();
}

uint16_t Parser::config_dgnmr(uint16_t pmu) const {
  return config.value().pmus[pmu].dginfo.size();
}

uint16_t Parser::deserialize_uint16_t() {
  uint16_t value;
  de_copy(&value);
  return ntohs(value);
}

uint32_t Parser::deserialize_uint32_t() {
  uint32_t value;
  de_copy(&value);
  return ntohl(value);
}

int16_t Parser::deserialize_int16_t() { return deserialize_uint16_t(); }

float Parser::deserialize_float() {
  uint32_t raw = deserialize_uint32_t();
  return *(float *)&raw;
}

std::string Parser::deserialize_name1() {
  std::string value(16, 0);
  de_copy<char>(value.data(), 16);
  return value;
}

Phasor Parser::deserialize_phasor(uint16_t pmu) {
  switch (config_format(pmu) & 0x3) {
  case 0x0: {
    auto real = deserialize_int16_t();
    auto imag = deserialize_int16_t();
    return Phasor::make<Phasor::RectangularInt>(real, imag);
  }

  case 0x1: {
    auto abs = deserialize_uint16_t();
    auto arg = deserialize_int16_t();
    return Phasor::make<Phasor::PolarInt>(abs, arg);
  }

  case 0x2: {
    auto real = deserialize_float();
    auto imag = deserialize_float();
    return Phasor::make<Phasor::RectangularFloat>(real, imag);
  }

  case 0x3: {
    auto abs = deserialize_float();
    auto arg = deserialize_float();
    return Phasor::make<Phasor::PolarFloat>(abs, arg);
  }

  default:
    throw RuntimeError{"c37_118: unknown phasor format"};
  }
}

Analog Parser::deserialize_analog(uint16_t pmu) {
  switch (config_format(pmu) & 0x4) {
  case 0x0: {
    auto value = deserialize_int16_t();
    return Analog::make<Analog::Int>(value);
  }

  case 0x4: {
    auto value = deserialize_float();
    return Analog::make<Analog::Float>(value);
  }

  default:
    throw RuntimeError{"c37_118: unknown analog format"};
  }
}

Freq Parser::deserialize_freq(uint16_t pmu) {
  switch (config_format(pmu) & 0x8) {
  case 0x0: {
    auto value = deserialize_int16_t();
    return Freq::make<Freq::Int>(value);
  }

  case 0x8: {
    auto value = deserialize_float();
    return Freq::make<Freq::Float>(value);
  }

  default:
    throw RuntimeError{"c37_118: unknown freq format"};
  }
}

PmuData Parser::deserialize_pmu_data(uint16_t pmu) {
  std::vector<Phasor> phasor(config_phnmr(pmu));
  std::vector<Analog> analog(config_annmr(pmu));
  std::vector<uint16_t> digital(config_dgnmr(pmu));

  auto stat = deserialize_uint16_t();
  for (auto &p : phasor)
    p = deserialize_phasor(pmu);
  auto freq = deserialize_freq(pmu);
  auto dfreq = deserialize_freq(pmu);
  for (auto &a : analog)
    a = deserialize_analog(pmu);
  for (auto &d : digital)
    d = deserialize_uint16_t();

  return {stat, phasor, freq, dfreq, analog, digital};
}

PmuConfig Parser::deserialize_pmu_config_simple() {
  auto stn = deserialize_name1();
  auto idcode = deserialize_uint16_t();
  auto format = deserialize_uint16_t();
  std::vector<ChannelInfo> phinfo(deserialize_uint16_t());
  std::vector<ChannelInfo> aninfo(deserialize_uint16_t());
  std::vector<DigitalInfo> dginfo(deserialize_uint16_t());
  for (auto &ph : phinfo)
    ph.nam = deserialize_name1();
  for (auto &an : aninfo)
    an.nam = deserialize_name1();
  for (auto &dg : dginfo)
    for (auto &nam : dg.nam)
      nam = deserialize_name1();
  for (auto &ph : phinfo)
    ph.unit = deserialize_uint32_t();
  for (auto &an : aninfo)
    an.unit = deserialize_uint32_t();
  for (auto &dg : dginfo)
    dg.unit = deserialize_uint32_t();
  auto fnom = deserialize_uint16_t();
  auto cfgcnt = deserialize_uint16_t();

  return {stn, idcode, format, phinfo, aninfo, dginfo, fnom, cfgcnt};
}

Config Parser::deserialize_config_simple() {
  auto time_base = deserialize_uint32_t();
  std::vector<PmuConfig> pmus(deserialize_uint16_t());
  for (auto &pmu : pmus)
    pmu = deserialize_pmu_config_simple();
  auto data_rate = deserialize_uint16_t();

  return Config{time_base, pmus, data_rate};
}

Config1 Parser::deserialize_config1() {
  return deserialize_config_simple();
}

Config2 Parser::deserialize_config2() {
  return deserialize_config_simple();
}

Data Parser::deserialize_data() {
  std::vector<PmuData> pmus;
  auto num_pmu = config_num_pmu();
  pmus.reserve(num_pmu);

  for (uint16_t i = 0; i < num_pmu; i++)
    pmus.push_back(deserialize_pmu_data(i));

  return {pmus};
}

Header Parser::deserialize_header() {
  auto data = std::string(de_cursor, de_end);

  return {data};
}

Command Parser::deserialize_command() {
  auto cmd = deserialize_uint16_t();
  auto ext = std::vector(de_cursor, de_end);

  return {cmd, ext};
}

std::optional<Frame> Parser::try_deserialize_frame() {
  auto de_begin = de_cursor;
  if (de_end - de_begin < 4)
    return std::nullopt;

  auto sync = deserialize_uint16_t();
  auto framesize = deserialize_uint16_t();

  if (de_end - de_begin < framesize)
    return std::nullopt;

  de_end = de_begin + framesize - sizeof(uint16_t);
  auto idcode = deserialize_uint16_t();
  auto soc = deserialize_uint32_t();
  auto fracsec = deserialize_uint32_t();

  if ((sync & 0xFF80) != 0xAA00)
    throw RuntimeError{"c37_118: invalid SYNC"};

  uint16_t version = sync & 0xF;
  Frame::Variant message;
  switch (sync & 0x70) {
  case 0x00:
    if (config.has_value())
      message = deserialize_data();
    break;
  case 0x10:
    message = deserialize_header();
    break;
  case 0x20:
    message = deserialize_config1();
    break;
  case 0x30:
    message = deserialize_config2();
    break;
  case 0x40:
    message = deserialize_command();
    break;
  default:
    throw RuntimeError{"c37_118: unsupported frame type"};
  }

  de_cursor = de_end;
  de_end += sizeof(uint16_t);
  auto crc = deserialize_uint16_t();
  auto expected_crc = calculate_crc(de_begin, framesize - sizeof(crc));
  if (crc != expected_crc)
    throw RuntimeError{"c37_118: checksum mismatch"};

  return {{version, framesize, idcode, soc, fracsec, message}};
}

void Parser::serialize_uint16_t(const uint16_t &value) {
  uint16_t i = htons(value);
  se_copy(&i);
}

void Parser::serialize_uint32_t(const uint32_t &value) {
  uint32_t i = htonl(value);
  se_copy(&i);
}

void Parser::serialize_int16_t(const int16_t &value) {
  serialize_uint16_t(value);
}

void Parser::serialize_float(const float &value) {
  uint32_t i = *(uint32_t *)&value;
  serialize_uint32_t(i);
}

void Parser::serialize_name1(const std::string &value) {
  std::string copy = value;
  copy.resize(16, ' ');
  se_copy(value.data(), 16);
}

void Parser::serialize_phasor(const Phasor &value, uint16_t pmu) {
  switch (config_format(pmu) & 0x3) {
  case 0x0: {
    auto [real, imag] = value.get<Phasor::RectangularInt>();
    serialize_int16_t(real);
    serialize_int16_t(imag);
    return;
  }

  case 0x1: {
    auto [abs, arg] = value.get<Phasor::PolarInt>();
    serialize_uint16_t(abs);
    serialize_int16_t(arg);
    return;
  }

  case 0x2: {
    auto [real, imag] = value.get<Phasor::RectangularFloat>();
    serialize_float(real);
    serialize_float(imag);
    return;
  }

  case 0x3: {
    auto [abs, arg] = value.get<Phasor::PolarFloat>();
    serialize_float(abs);
    serialize_float(arg);
    return;
  }

  default:
    throw RuntimeError{"c37_118: unknown phasor format"};
  }
}

void Parser::serialize_analog(const Analog &value, uint16_t pmu) {
  switch (config_format(pmu) & 0x4) {
  case 0x0: {
    serialize_int16_t(value.get<Analog::Int>());
    return;
  }

  case 0x4: {
    serialize_float(value.get<Analog::Float>());
    return;
  }

  default:
    throw RuntimeError{"c37_118: unknown analog format"};
  }
}

void Parser::serialize_freq(const Freq &value, uint16_t pmu) {
  switch (config_format(pmu) & 0x8) {
  case 0x0: {
    serialize_int16_t(value.get<Freq::Int>());
    return;
  }

  case 0x8: {
    serialize_float(value.get<Freq::Float>());
    return;
  }

  default:
    throw RuntimeError{"c37_118: unknown freq format"};
  }
}

void Parser::serialize_pmu_data(const PmuData &value, uint16_t pmu) {
  if (value.phasor.size() != config_phnmr(pmu))
    throw RuntimeError{"c37_118: [phasor] expected [{}], got [{}]",
                       config_phnmr(pmu), value.phasor.size()};

  if (value.analog.size() != config_annmr(pmu))
    throw RuntimeError{"c37_118: [analog] expected [{}], got [{}]",
                       config_annmr(pmu), value.analog.size()};

  if (value.digital.size() != config_dgnmr(pmu))
    throw RuntimeError{"c37_118: [digital] expected [{}], got [{}]",
                       config_dgnmr(pmu), value.digital.size()};

  serialize_uint16_t(value.stat);
  for (auto &ph : value.phasor)
    serialize_phasor(ph, pmu);
  serialize_freq(value.freq, pmu);
  serialize_freq(value.dfreq, pmu);
  for (auto &an : value.analog)
    serialize_analog(an, pmu);
  for (auto &dg : value.digital)
    serialize_uint16_t(dg);
}

void Parser::serialize_pmu_config_simple(const PmuConfig &value) {
  serialize_name1(value.stn);
  serialize_uint16_t(value.idcode);
  serialize_uint16_t(value.format);
  serialize_uint16_t(value.phinfo.size());
  serialize_uint16_t(value.aninfo.size());
  serialize_uint16_t(value.dginfo.size());
  for (auto &ph : value.phinfo)
    serialize_name1(ph.nam);
  for (auto &an : value.aninfo)
    serialize_name1(an.nam);
  for (auto &dg : value.dginfo)
    for (auto &nam : dg.nam)
      serialize_name1(nam);
  for (auto &ph : value.phinfo)
    serialize_uint32_t(ph.unit);
  for (auto &an : value.aninfo)
    serialize_uint32_t(an.unit);
  for (auto &dg : value.dginfo)
    serialize_uint32_t(dg.unit);
  serialize_uint16_t(value.fnom);
  serialize_uint16_t(value.cfgcnt);
}

void Parser::serialize_config_simple(const Config &value) {
  serialize_uint32_t(value.time_base);
  serialize_uint16_t(value.pmus.size());
  for (auto &pmu : value.pmus)
    serialize_pmu_config_simple(pmu);
  serialize_uint16_t(value.data_rate);
}

void Parser::serialize_config1(const Config1 &value) {
  serialize_config_simple(value);
}

void Parser::serialize_config2(const Config2 &value) {
  serialize_config_simple(value);
}

void Parser::serialize_data(const Data &value) {
  if (value.pmus.size() != config_num_pmu())
    throw RuntimeError{"c37_118: [pmus] expected {}, got {}", config_num_pmu(),
                       value.pmus.size()};

  for (uint16_t i = 0; i < value.pmus.size(); i++)
    serialize_pmu_data(value.pmus[i], i);
}

void Parser::serialize_header(const Header &value) {
  se_copy(value.data.data(), value.data.size());
}

void Parser::serialize_command(const Command &value) {
  serialize_uint16_t(value.cmd);
  se_copy(value.ext.data(), value.ext.size());
}

void Parser::serialize_frame(const Frame &value) {
  uint16_t sync = 0xAA00 | ((value.message.index()) << 4) | value.version;

  serialize_uint16_t(sync);
  serialize_uint16_t(0); // framesize placeholder
  serialize_uint16_t(value.idcode);
  serialize_uint32_t(value.soc);
  serialize_uint32_t(value.fracsec);

  std::visit(villas::utils::overloaded{
                 [&](Data const &data) { serialize_data(data); },
                 [&](Header const &header) { serialize_header(header); },
                 [&](Config1 const &config1) { serialize_config1(config1); },
                 [&](Config2 const &config2) { serialize_config2(config2); },
                 [&](Command const &command) { serialize_command(command); },
                 [](std::monostate) {
                   throw RuntimeError{"c37_118: [frame] missing message"};
                 },
             },
             value.message);

  auto framesize = htons(se_buffer.size() + sizeof(uint16_t));
  std::memcpy(se_buffer.data() + sizeof(sync), &framesize, sizeof(framesize));
  auto crc = calculate_crc(se_buffer.data(), se_buffer.size());
  serialize_uint16_t(crc);
}
