/* Parser for C37-118.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2024 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>
#include <cmath>
#include <villas/exceptions.hpp>
#include <villas/nodes/c37_118/parser.hpp>
#include <villas/utils.hpp>

using namespace villas::node::c37_118;
using namespace villas::node::c37_118::parser;

// Sample CRC routine taken from IEEE Std C37.118.2-2011 Annex B
//
// TODO: Is this code license compatible to Apache 2.0?
uint16_t parser::calculate_crc(const unsigned char *frame, uint16_t size) {
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

std::optional<Frame> Parser::deserialize(const unsigned char *buffer,
                                         std::size_t length,
                                         const Config *config) {
  de_cursor = buffer;
  de_end = buffer + length;
  return try_deserialize_frame(config);
}

std::vector<unsigned char> Parser::serialize(const Frame &frame,
                                             const Config *config) {
  se_buffer.clear();
  serialize_frame(frame, config);
  return se_buffer;
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

std::complex<float> Parser::deserialize_phasor(uint16_t format, const PhasorInfo &phinfo) {
  switch (format & 0x3) {
  case 0x0: {
    auto real = static_cast<float>(deserialize_int16_t()) * phinfo.scale();
    auto imag = static_cast<float>(deserialize_int16_t()) * phinfo.scale();
    return std::complex(real, imag);
  }

  case 0x1: {
    auto abs = static_cast<float>(deserialize_uint16_t()) * phinfo.scale();
    auto arg = static_cast<float>(deserialize_int16_t()) / 10'000;
    return std::polar(abs, arg);
  }

  case 0x2: {
    auto real = deserialize_float();
    auto imag = deserialize_float();
    return std::complex(real, imag);
  }

  case 0x3: {
    auto abs = deserialize_float();
    auto arg = deserialize_float();
    return std::polar(abs, arg);
  }

  default:
    throw RuntimeError{"c37_118: unknown phasor format"};
  }
}

float Parser::deserialize_freq(const PmuConfig &pmu) {
  switch (pmu.format & 0x8) {
  case 0x0: {
    auto fnom = pmu.fnom & 1 ? 50.0 : 60.0;
    return (static_cast<float>(deserialize_int16_t()) / 1'000) + fnom;
  }

  case 0x8: {
    return deserialize_float();
  }

  default:
    throw RuntimeError{"c37_118: unknown freq format"};
  }
}

float Parser::deserialize_dfreq(uint16_t format) {
  switch (format & 0x8) {
  case 0x0: {
    return static_cast<float>(deserialize_int16_t()) / 100;
  }

  case 0x8: {
    return deserialize_float();
  }

  default:
    throw RuntimeError{"c37_118: unknown freq format"};
  }
}

float Parser::deserialize_analog(uint16_t format, const AnalogInfo &aninfo) {
  switch (format & 0x4) {
  case 0x0: {
    return static_cast<float>(deserialize_int16_t()) * aninfo.scale();
  }

  case 0x4: {
    return deserialize_float(); // * aninfo.scale() ?????
  }

  default:
    throw RuntimeError{"c37_118: unknown analog format"};
  }
}

uint16_t Parser::deserialize_digital(const DigitalInfo &dginfo) {
  auto normal = dginfo.dgunit >> 16;
  auto valid = dginfo.dgunit & 0xFF;
  return (deserialize_uint16_t() ^ normal) & valid;
}

PmuData Parser::deserialize_pmu_data(const PmuConfig &pmu_config) {
  std::vector<std::complex<float>> phasor(pmu_config.phinfo.size());
  std::vector<float> analog(pmu_config.aninfo.size());
  std::vector<uint16_t> digital(pmu_config.dginfo.size());

  auto stat = deserialize_uint16_t();
  for (size_t i = 0; i < phasor.size(); ++i)
    phasor[i] = deserialize_phasor(pmu_config.format, pmu_config.phinfo[i]);
  auto freq = deserialize_freq(pmu_config);
  auto dfreq = deserialize_dfreq(pmu_config.format);
  for (size_t i = 0; i < analog.size(); ++i)
    analog[i] = deserialize_analog(pmu_config.format, pmu_config.aninfo[i]);
  for (size_t i = 0; i < digital.size(); ++i)
    digital[i] = deserialize_digital(pmu_config.dginfo[i]);

  return {stat, phasor, freq, dfreq, analog, digital};
}

PmuConfig Parser::deserialize_pmu_config_simple() {
  auto stn = deserialize_name1();
  auto idcode = deserialize_uint16_t();
  auto format = deserialize_uint16_t();
  std::vector<PhasorInfo> phinfo(deserialize_uint16_t());
  std::vector<AnalogInfo> aninfo(deserialize_uint16_t());
  std::vector<DigitalInfo> dginfo(deserialize_uint16_t());

  for (auto &ph : phinfo)
    ph.chnam = deserialize_name1();
  for (auto &an : aninfo)
    an.chnam = deserialize_name1();
  for (auto &dg : dginfo)
    for (auto &nam : dg.chnam)
      nam = deserialize_name1();

  for (auto &ph : phinfo)
    ph.phunit = deserialize_uint32_t();
  for (auto &an : aninfo)
    an.anunit = deserialize_uint32_t();
  for (auto &dg : dginfo)
    dg.dgunit = deserialize_uint32_t();

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

Config1 Parser::deserialize_config1() { return deserialize_config_simple(); }

Config2 Parser::deserialize_config2() { return deserialize_config_simple(); }

Data Parser::deserialize_data(const Config &config) {
  std::vector<PmuData> pmus;
  pmus.reserve(config.pmus.size());
  for (const auto &pmu_config : config.pmus)
    pmus.push_back(deserialize_pmu_data(pmu_config));

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

std::optional<Frame> Parser::try_deserialize_frame(const Config *config) {
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
    if (config)
      message = deserialize_data(*config);
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

void Parser::serialize_phasor(const std::complex<float> &value, uint16_t format, const PhasorInfo &phinfo) {
  switch (format & 0x3) {
  case 0x0: {
    serialize_int16_t(static_cast<int16_t>(value.real() / phinfo.scale()));
    serialize_int16_t(static_cast<int16_t>(value.imag() / phinfo.scale()));
    return;
  }

  case 0x1: {
    serialize_uint16_t(static_cast<uint16_t>(std::abs(value) / phinfo.scale()));
    serialize_int16_t(static_cast<uint16_t>(std::arg(value) * 10'000));
    return;
  }

  case 0x2: {
    serialize_float(value.real());
    serialize_float(value.imag());
    return;
  }

  case 0x3: {
    serialize_float(std::abs(value));
    serialize_float(std::arg(value));
    return;
  }

  default:
    throw RuntimeError{"c37_118: unknown phasor format"};
  }
}

void Parser::serialize_freq(const float &value, const PmuConfig &pmu) {
  switch (pmu.format & 0x8) {
  case 0x0: {
    auto fnom = pmu.fnom & 1 ? 50.0 : 60.0;
    serialize_int16_t(static_cast<int16_t>((value - fnom) * 1'000));
    return;
  }

  case 0x8: {
    serialize_float(value);
    return;
  }

  default:
    throw RuntimeError{"c37_118: unknown freq format"};
  }
}

void Parser::serialize_dfreq(const float &value, uint16_t format) {
  switch (format & 0x8) {
  case 0x0: {
    serialize_int16_t(static_cast<int16_t>(value * 100));
    return;
  }

  case 0x8: {
    serialize_float(value);
    return;
  }

  default:
    throw RuntimeError{"c37_118: unknown freq format"};
  }
}

void Parser::serialize_analog(const float &value, uint16_t format, const AnalogInfo &aninfo) {
  switch (format & 0x4) {
  case 0x0: {
    serialize_int16_t(static_cast<int16_t>(value / aninfo.scale()));
    return;
  }

  case 0x4: {
    serialize_float(value);
    return;
  }

  default:
    throw RuntimeError{"c37_118: unknown analog format"};
  }
}

void Parser::serialize_digital(const uint16_t &value, const DigitalInfo &dginfo) {
  auto normal = dginfo.dgunit >> 16;
  auto valid = dginfo.dgunit & 0xFF;
  return serialize_uint16_t((value ^ normal) & valid);
}

void Parser::serialize_pmu_data(const PmuData &value, const PmuConfig &pmu_config) {
  if (value.phasor.size() != pmu_config.phinfo.size())
    throw RuntimeError{"c37_118: [phasor] expected [{}], got [{}]",
                       pmu_config.phinfo.size(), value.phasor.size()};

  if (value.analog.size() != pmu_config.aninfo.size())
    throw RuntimeError{"c37_118: [analog] expected [{}], got [{}]",
                       pmu_config.aninfo.size(), value.analog.size()};

  if (value.digital.size() != pmu_config.dginfo.size())
    throw RuntimeError{"c37_118: [digital] expected [{}], got [{}]",
                       pmu_config.dginfo.size(), value.digital.size()};

  serialize_uint16_t(value.stat);
  for (size_t i = 0; i < value.phasor.size(); ++i)
    serialize_phasor(value.phasor[i], pmu_config.format, pmu_config.phinfo[i]);
  serialize_freq(value.freq, pmu_config);
  serialize_dfreq(value.dfreq, pmu_config.format);
  for (size_t i = 0; i < value.analog.size(); ++i)
    serialize_analog(value.analog[i], pmu_config.format, pmu_config.aninfo[i]);
  for (size_t i = 0; i < value.digital.size(); ++i)
    serialize_digital(value.digital[i], pmu_config.dginfo[i]);
}

void Parser::serialize_pmu_config_simple(const PmuConfig &value) {
  serialize_name1(value.stn);
  serialize_uint16_t(value.idcode);
  serialize_uint16_t(value.format);
  serialize_uint16_t(value.phinfo.size());
  serialize_uint16_t(value.aninfo.size());
  serialize_uint16_t(value.dginfo.size());

  for (auto &ph : value.phinfo)
    serialize_name1(ph.chnam);
  for (auto &an : value.aninfo)
    serialize_name1(an.chnam);
  for (auto &dg : value.dginfo)
    for (auto &nam : dg.chnam)
      serialize_name1(nam);

  for (auto &ph : value.phinfo)
    serialize_uint32_t(ph.phunit);
  for (auto &an : value.aninfo)
    serialize_uint32_t(an.anunit);
  for (auto &dg : value.dginfo)
    serialize_uint32_t(dg.dgunit);

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

void Parser::serialize_data(const Data &value, const Config &config) {
  if (value.pmus.size() != config.pmus.size())
    throw RuntimeError{"c37_118: [pmus] expected {}, got {}", config.pmus.size(),
                       value.pmus.size()};

  for (uint16_t i = 0; i < value.pmus.size(); i++)
    serialize_pmu_data(value.pmus[i], config.pmus[i]);
}

void Parser::serialize_header(const Header &value) {
  se_copy(value.data.data(), value.data.size());
}

void Parser::serialize_command(const Command &value) {
  serialize_uint16_t(value.cmd);
  se_copy(value.ext.data(), value.ext.size());
}

void Parser::serialize_frame(const Frame &value, const Config *config) {
  auto version = 1;
  uint16_t sync = 0xAA00 | ((value.message.index()) << 4) | version;

  serialize_uint16_t(sync);
  serialize_uint16_t(0); // framesize placeholder
  serialize_uint16_t(value.idcode);
  serialize_uint32_t(value.soc);
  serialize_uint32_t(value.fracsec);

  std::visit(villas::utils::overloaded{
                 [&](const Data &data) {
                   if (config) serialize_data(data, *config);
                   else throw RuntimeError{"c37_118: [frame] missing configuration for data frame"};
                 },
                 [&](const Header &header) { serialize_header(header); },
                 [&](const Config1 &config1) { serialize_config1(config1); },
                 [&](const Config2 &config2) { serialize_config2(config2); },
                 [&](const Command &command) { serialize_command(command); },
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
