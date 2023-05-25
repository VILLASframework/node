/* Node type: C37-118.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2024 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <array>
#include <complex>
#include <stdint.h>
#include <variant>
#include <vector>

using namespace std::literals;

namespace villas::node::c37_118::types {

struct PmuData final {
  uint16_t stat;
  std::vector<std::complex<float>> phasor;
  float freq;
  float dfreq;
  std::vector<float> analog;
  std::vector<uint16_t> digital;
};

struct Data final {
  std::vector<PmuData> pmus;
};

struct Header final {
  std::string data;
};

struct PhasorInfo final {
  static constexpr uint8_t UNIT_VOLT = 0;
  static constexpr uint8_t UNIT_AMPERE = 1;

  std::string chnam;
  uint32_t phunit;

  uint8_t unit() const noexcept {
    return phunit >> 24;
  }

  std::string unit_str() const noexcept {
    switch (unit()) {
      case UNIT_VOLT:
        return "volt"s;
      case UNIT_AMPERE:
        return "ampere"s;
      default:
        return "other"s;
    }
  }

  float scale() const noexcept {
    return static_cast<float>(phunit & 0xFFFFFF) / 10'000;
  }
};

struct AnalogInfo final {
  static constexpr uint8_t UNIT_POINT_ON_WAVE = 0;
  static constexpr uint8_t UNIT_RMS = 1;
  static constexpr uint8_t UNIT_PEAK = 2;

  std::string chnam;
  uint32_t anunit;

  uint8_t unit() const noexcept {
    return anunit >> 24;
  }

  std::string unit_str() const noexcept {
    switch (unit()) {
      case UNIT_POINT_ON_WAVE:
        return "point-on-wave"s;
      case UNIT_RMS:
        return "rms"s;
      case UNIT_PEAK:
        return "peak"s;
      default:
        return "other"s;
    }
  }

  float scale() const noexcept {
    return static_cast<float>(anunit & 0xFFFFFF);
  }
};

struct DigitalInfo final {
  std::array<std::string, 16> chnam;
  uint32_t dgunit;
};

struct PmuConfig final {
  std::string stn;
  uint16_t idcode;
  uint16_t format;
  std::vector<PhasorInfo> phinfo;
  std::vector<AnalogInfo> aninfo;
  std::vector<DigitalInfo> dginfo;
  uint16_t fnom;
  uint16_t cfgcnt;
};

struct Config {
  uint32_t time_base;
  std::vector<PmuConfig> pmus;
  uint16_t data_rate;
};

class Config1 {
private:
  Config inner;

public:
  Config1() = delete;
  Config1(Config config) noexcept : inner(config) {}
  operator Config &() noexcept { return inner; }
  operator Config const &() const noexcept { return inner; }
  Config *operator->() { return &inner; }
  Config const *operator->() const { return &inner; }
};

class Config2 {
private:
  Config inner;

public:
  Config2() = delete;
  Config2(Config config) noexcept : inner(config) {}
  operator Config &() noexcept { return inner; }
  operator Config const &() const noexcept { return inner; }
  Config *operator->() { return &inner; }
  Config const *operator->() const { return &inner; }
};

struct Command final {
  uint16_t cmd;
  std::vector<unsigned char> ext;

  static constexpr uint16_t DATA_STOP = 0x1;
  static constexpr uint16_t DATA_START = 0x2;
  static constexpr uint16_t GET_HEADER = 0x3;
  static constexpr uint16_t GET_CONFIG1 = 0x4;
  static constexpr uint16_t GET_CONFIG2 = 0x5;
  //static constexpr uint16_t GET_CONFIG3 = 0x6;
};

struct Frame final {
  using Variant = std::variant<Data, Header, Config1, Config2, Command>;

  uint16_t version;
  uint16_t framesize;
  uint16_t idcode;
  uint32_t soc;
  uint32_t fracsec;
  Variant message;
};

} // namespace villas::node::c37_118::types
