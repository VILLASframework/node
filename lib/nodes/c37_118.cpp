/* C37.118.2 PMU/PDC communication protocol implementation.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <netdb.h>
#include <nlohmann/json.hpp>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <uuid.h>

#include <villas/exceptions.hpp>
#include <villas/json.hpp>
#include <villas/node.hpp>
#include <villas/nodes/c37_118.hpp>
#include <villas/queue_signalled.h>
#include <villas/sample.hpp>
#include <villas/timing.hpp>

namespace villas::node {

void from_json(Json const &json, C37PhasorUnit &unit) {
  auto const &str = json.get_ref<Json::string_t const &>();
  if (str == "volt")
    unit = C37PhasorUnit::VOLT;
  else if (str == "ampere")
    unit = C37PhasorUnit::AMPERE;
  else
    throw RuntimeError("Invalid phasor unit '{}'", str);
}

void from_json(Json const &json, C37AnalogUnit &unit) {
  auto const &str = json.get_ref<Json::string_t const &>();
  if (str == "point_on_wave")
    unit = C37AnalogUnit::POINT_ON_WAVE;
  else if (str == "rms")
    unit = C37AnalogUnit::RMS;
  else if (str == "peak")
    unit = C37AnalogUnit::PEAK;
  else
    throw RuntimeError("Invalid analog unit '{}'", str);
}

void from_json(Json const &json, C37PhasorComponent &component) {
  auto const &str = json.get_ref<Json::string_t const &>();
  if (str == "zero_sequence")
    component = C37PhasorComponent::ZERO_SEQUENCE;
  else if (str == "positive_sequence")
    component = C37PhasorComponent::POSITIVE_SEQUENCE;
  else if (str == "negative_sequence")
    component = C37PhasorComponent::NEGATIVE_SEQUENCE;
  else if (str == "phase_a")
    component = C37PhasorComponent::PHASE_A;
  else if (str == "phase_b")
    component = C37PhasorComponent::PHASE_B;
  else if (str == "phase_c")
    component = C37PhasorComponent::PHASE_C;
  else
    throw RuntimeError("Invalid phasor component '{}'", str);
}

void from_json(Json const &json, C37ServiceClass &service_class) {
  auto const &str = json.get_ref<Json::string_t const &>();
  if (str == "measurement")
    service_class = C37ServiceClass::M;
  else if (str == "protection")
    service_class = C37ServiceClass::P;
  else
    throw RuntimeError("Invalid service class '{}'", str);
}

namespace {

// A Sample CRC routine taken from IEEE Std C37.118.2-2011 Annex B.
uint16_t calculate_crc(std::span<const std::byte> buffer) {
  uint16_t crc = 0xFFFF;
  uint16_t temp;
  uint16_t quick;

  for (auto byte : buffer) {
    temp = (crc >> 8) ^ static_cast<uint16_t>(byte);
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

template <typename T, std::size_t shift, std::size_t mask>
class bit_field_proxy {
  T &value;

public:
  constexpr bit_field_proxy(T &value) : value(value) {}

  constexpr T get() const { return (value >> shift) & mask; }

  constexpr operator T() const { return get(); }

  constexpr void set(T other) {
    value &= ~(mask << shift);
    value |= (other & mask) << shift;
  }

  constexpr bit_field_proxy &operator=(T other) {
    set(other);
    return *this;
  }

  template <typename To> constexpr To as() const {
    return std::bit_cast<To>(get());
  }
};

template <std::size_t high, std::size_t low = high, typename T>
  requires(std::is_integral_v<T> and std::is_unsigned_v<T> and
           high < sizeof(T) * CHAR_BIT and high >= low)
constexpr bit_field_proxy<T, low, (1 << (high - low + 1)) - 1>
bit_field(T &value) {
  return value;
}

constexpr C37Result c37_result_request(std::size_t size) {
  using underlying_type = std::underlying_type_t<C37Result>;

  constexpr auto minimum =
      static_cast<underlying_type>(C37Result::_requested_size_minimum);

  if (size < minimum or not std::in_range<underlying_type>(size)) {
    return C37Result::INVALID_FRAMESIZE;
  }

  return static_cast<C37Result>(static_cast<underlying_type>(size));
}

template <typename T>
constexpr T from_be_bytes_unchecked(std::byte const *bytes,
                                    std::uint16_t &offset) {
  if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
    T temp = 0;

    for (auto const i : std::views::iota(size_t(0), sizeof(T)))
      temp = temp << 8 | static_cast<T>(bytes[offset + i]);

    offset += sizeof(T);

    return temp;
  } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
    return std::bit_cast<T>(
        from_be_bytes_unchecked<std::make_unsigned_t<T>>(bytes, offset));
  } else if constexpr (std::is_same_v<T, float>) {
    static_assert(sizeof(float) == sizeof(uint32_t));
    return std::bit_cast<float>(
        from_be_bytes_unchecked<uint32_t>(bytes, offset));
  } else {
    return static_cast<T>(
        from_be_bytes_unchecked<std::underlying_type_t<T>>(bytes, offset));
  }
}

template <typename... T>
  requires(... and (std::is_integral_v<T> or std::is_same_v<T, float> or
                    std::is_enum_v<T>))
C37Result deserialize(std::span<const std::byte> buffer, std::uint16_t &offset,
                      T &...value) {

  if (auto required_size = offset + (... + sizeof(T));
      required_size > buffer.size())
    return c37_result_request(required_size);

  ((void)(value = from_be_bytes_unchecked<T>(buffer.data(), offset)), ...);

  return C37Result::OK;
}

C37Result deserialize_fixed_string(std::span<const std::byte> message,
                                   std::uint16_t &offset,
                                   std::string &fixed_string) {
  if (message.size() < size_t(offset + 16))
    return c37_result_request(16);

  auto view = std::string_view(
      reinterpret_cast<char const *>(message.data() + offset), 16);
  auto begin = view.find_first_not_of(" ");
  auto end = view.find_last_not_of(" ");

  if (begin == std::string_view::npos)
    fixed_string.clear();
  else
    fixed_string = view.substr(begin, end - begin + 1);

  offset += 16;

  return C37Result::OK;
}

C37Result deserialize_dynamic_string(std::span<const std::byte> message,
                                     std::uint16_t &offset,
                                     std::string &dynamic_string) {
  uint8_t length;
  if (auto result = deserialize(message, offset, length);
      result != C37Result::OK)
    return result;

  if (message.size() < size_t(offset + length))
    return c37_result_request(offset + length);

  dynamic_string.assign(reinterpret_cast<char const *>(message.data() + offset),
                        length);
  offset += length;

  return C37Result::OK;
}

C37Result deserialize_phasor(std::span<const std::byte> message,
                             C37Pmu const &pmu, C37PhasorInfo const &info,
                             std::uint16_t &offset,
                             std::complex<float> &phasor) {
  if (pmu.phasor_is_float) {
    float first, second;
    if (auto result = deserialize(message, offset, first, second);
        result != C37Result::OK)
      return result;

    if (pmu.phasor_is_polar) {
      phasor = std::polar(first, second);
    } else {
      phasor = std::complex(first, second);
    }
  } else {
    int16_t first, second;
    if (auto result = deserialize(message, offset, first, second);
        result != C37Result::OK)
      return result;

    if (pmu.phasor_is_polar) {
      auto abs = static_cast<float>(std::bit_cast<uint16_t>(first)) *
                 info.amplitude_scale;
      auto arg = (static_cast<float>(second) / 10'000) - info.phase_shift;
      phasor = std::polar(abs, arg);
    } else {
      auto real = first * info.amplitude_scale;
      auto imag = second * info.amplitude_scale;
      phasor = std::complex(real, imag);

      if (info.phase_shift != 0)
        phasor *= std::polar<float>(1, -info.phase_shift);
    }
  }

  return C37Result::OK;
}

C37Result deserialize_freq(std::span<const std::byte> message,
                           C37Pmu const &pmu, std::uint16_t &offset,
                           float &freq) {
  if (pmu.freq_is_float) {
    if (auto result = deserialize(message, offset, freq);
        result != C37Result::OK)
      return result;
  } else {
    int16_t value;
    if (auto result = deserialize(message, offset, value);
        result != C37Result::OK)
      return result;

    freq = static_cast<float>(value) / 1'000 + pmu.nominal_frequency;
  }

  return C37Result::OK;
}

C37Result deserialize_dfreq(std::span<const std::byte> message,
                            C37Pmu const &pmu, std::uint16_t &offset,
                            float &dfreq) {
  if (pmu.freq_is_float) {
    if (auto result = deserialize(message, offset, dfreq);
        result != C37Result::OK)
      return result;
  } else {
    int16_t value;
    if (auto result = deserialize(message, offset, value);
        result != C37Result::OK)
      return result;

    dfreq = static_cast<float>(value) / 100;
  }

  return C37Result::OK;
}

C37Result deserialize_analog(std::span<const std::byte> message,
                             C37Pmu const &pmu, C37AnalogInfo const &info,
                             std::uint16_t &offset, float &analog) {
  if (pmu.analog_is_float) {
    if (auto result = deserialize(message, offset, analog);
        result != C37Result::OK)
      return result;
  } else {
    int16_t value;
    if (auto result = deserialize(message, offset, value);
        result != C37Result::OK)
      return result;

    analog = static_cast<float>(value) * info.scale;
  }

  return C37Result::OK;
}

C37Result deserialize_digital(std::span<const std::byte> message,
                              C37Pmu const &pmu, C37DigitalInfo const &info,
                              std::uint16_t &offset, uint16_t &digital) {
  if (auto result = deserialize(message, offset, digital);
      result != C37Result::OK)
    return result;

  digital &= info.mask;

  return C37Result::OK;
}

C37Result deserialize_pmu_data(std::span<const std::byte> message,
                               C37Pmu const &pmu, std::uint16_t &offset,
                               C37Data &data) {
  uint16_t stat;
  if (auto result = deserialize(message, offset, stat); result != C37Result::OK)
    return result;

  data.data_error = bit_field<13>(stat);
  data.out_of_sync = bit_field<12>(stat);
  data.sorted_by_arrival = bit_field<11>(stat);
  data.trigger_detected = bit_field<10>(stat);
  data.configuration_change = bit_field<9>(stat);
  data.time_error = bit_field<8, 6>(stat).as<C37DataTimeError>();
  data.unlocked_time = bit_field<5, 4>(stat).as<C37DataUnlockedTime>();
  data.trigger_reason = bit_field<3, 0>(stat).as<C37DataTriggerReason>();

  data.phasor.resize(pmu.phasor.size());
  data.analog.resize(pmu.analog.size());
  data.digital.resize(pmu.digital.size());

  for (auto const i : std::views::iota(size_t(0), pmu.phasor.size())) {
    if (auto result = deserialize_phasor(message, pmu, pmu.phasor[i], offset,
                                         data.phasor[i]);
        result != C37Result::OK)
      return result;
  }

  if (auto result = deserialize_freq(message, pmu, offset, data.freq);
      result != C37Result::OK)
    return result;

  if (auto result = deserialize_dfreq(message, pmu, offset, data.dfreq);
      result != C37Result::OK)
    return result;

  for (auto const i : std::views::iota(size_t(0), pmu.analog.size())) {
    if (auto result = deserialize_analog(message, pmu, pmu.analog[i], offset,
                                         data.analog[i]);
        result != C37Result::OK)
      return result;
  }

  for (auto const i : std::views::iota(size_t(0), pmu.digital.size())) {
    if (auto result = deserialize_digital(message, pmu, pmu.digital[i], offset,
                                          data.digital[i]);
        result != C37Result::OK)
      return result;
  }

  return C37Result::OK;
}

C37Result deserialize_r1_pmu_config(std::span<const std::byte> message,
                                    std::uint16_t &offset, C37Pmu &pmu) {
  uint16_t phasor_count, analog_count, digital_count;
  uint16_t format;

  uuid_clear(pmu.guid);
  pmu.latitude = std::numeric_limits<float>::quiet_NaN();
  pmu.longitude = std::numeric_limits<float>::quiet_NaN();
  pmu.elevation = std::numeric_limits<float>::quiet_NaN();
  pmu.service_class = C37ServiceClass::M;
  pmu.window = 0;
  pmu.group_delay = 0;

  if (auto result = deserialize_fixed_string(message, offset, pmu.name);
      result != C37Result::OK)
    return result;

  if (auto result = deserialize(message, offset, pmu.idcode, format,
                                phasor_count, analog_count, digital_count);
      result != C37Result::OK)
    return result;

  pmu.freq_is_float = bit_field<3>(format);
  pmu.analog_is_float = bit_field<2>(format);
  pmu.phasor_is_float = bit_field<1>(format);
  pmu.phasor_is_polar = bit_field<0>(format);
  pmu.phasor.resize(phasor_count);
  pmu.analog.resize(analog_count);
  pmu.digital.resize(digital_count);

  for (auto &phasor : pmu.phasor) {
    if (auto result = deserialize_fixed_string(message, offset, phasor.name);
        result != C37Result::OK)
      return result;
  }

  for (auto &analog : pmu.analog) {
    if (auto result = deserialize_fixed_string(message, offset, analog.name);
        result != C37Result::OK)
      return result;
  }

  for (auto &digital : pmu.digital) {
    for (auto &name : digital.name) {
      if (auto result = deserialize_fixed_string(message, offset, name);
          result != C37Result::OK)
        return result;
    }
  }

  for (auto &phasor : pmu.phasor) {
    uint32_t phunit;
    if (auto result = deserialize(message, offset, phunit);
        result != C37Result::OK)
      return result;

    phasor.unit = bit_field<31, 24>(phunit).as<C37PhasorUnit>();
    phasor.component = C37PhasorComponent::PHASE_A;
    phasor.amplitude_scale = bit_field<23, 0>(phunit);
    phasor.phase_shift = 0;
    phasor.upsampled_with_interpolation = false;
    phasor.upsampled_with_extrapolation = false;
    phasor.downsampled_with_reselection = false;
    phasor.downsampled_with_fir_filter = false;
    phasor.downsampled_with_non_fir_filter = false;
    phasor.filtered_without_resampling = false;
    phasor.magnitude_adjusted_for_calibration = false;
    phasor.phase_adjusted_for_calibration = false;
    phasor.phase_adjusted_for_rotation = false;
    phasor.pseudo_phasor_value = false;
    phasor.other_modification = false;
  }

  for (auto &analog : pmu.analog) {
    uint32_t anunit;
    if (auto result = deserialize(message, offset, anunit);
        result != C37Result::OK)
      return result;

    analog.unit = bit_field<31, 24>(anunit).as<C37AnalogUnit>();
    analog.scale = bit_field<23, 0>(anunit);
    analog.offset = 0;
  }

  for (auto &digital : pmu.digital) {
    uint32_t dgunit;
    if (auto result = deserialize(message, offset, dgunit);
        result != C37Result::OK)
      return result;

    digital.normal = bit_field<31, 16>(dgunit);
    digital.mask = bit_field<15, 0>(dgunit);
  }

  uint16_t nominal_frequency_flags;
  if (auto result =
          deserialize(message, offset, nominal_frequency_flags, pmu.cfgcnt);
      result != C37Result::OK)
    return result;

  pmu.nominal_frequency = bit_field<0>(nominal_frequency_flags) ? 50.0f : 60.0f;

  return C37Result::OK;
}

C37Result deserialize_uuid(std::span<const std::byte> message,
                           std::uint16_t &offset, uuid_t &uuid) {
  if (message.size() < size_t(offset + sizeof(uuid_t)))
    return c37_result_request(offset + sizeof(uuid_t));

  std::memcpy(uuid, message.data() + offset, sizeof(uuid_t));
  offset += sizeof(uuid_t);

  return C37Result::OK;
}

C37Result deserialize_r2_phasor_scale(std::span<const std::byte> message,
                                      std::uint16_t &offset,
                                      C37PhasorInfo &phasor) {
  uint32_t flags;
  if (auto result = deserialize(message, offset, flags, phasor.amplitude_scale,
                                phasor.phase_shift);
      result != C37Result::OK)
    return result;

  phasor.other_modification = bit_field<31>(flags);
  phasor.pseudo_phasor_value = bit_field<26>(flags);
  phasor.phase_adjusted_for_rotation = bit_field<25>(flags);
  phasor.phase_adjusted_for_calibration = bit_field<24>(flags);
  phasor.magnitude_adjusted_for_calibration = bit_field<23>(flags);
  phasor.filtered_without_resampling = bit_field<22>(flags);
  phasor.downsampled_with_non_fir_filter = bit_field<21>(flags);
  phasor.downsampled_with_fir_filter = bit_field<20>(flags);
  phasor.downsampled_with_reselection = bit_field<19>(flags);
  phasor.upsampled_with_extrapolation = bit_field<18>(flags);
  phasor.upsampled_with_interpolation = bit_field<17>(flags);
  phasor.unit = bit_field<11>(flags).as<C37PhasorUnit>();
  phasor.component = bit_field<10, 8>(flags).as<C37PhasorComponent>();

  return C37Result::OK;
}

C37Result deserialize_r2_analog_scale(std::span<const std::byte> message,
                                      std::uint16_t &offset,
                                      C37AnalogInfo &analog) {
  if (auto result = deserialize(message, offset, analog.scale, analog.offset);
      result != C37Result::OK)
    return result;

  analog.unit = C37AnalogUnit::POINT_ON_WAVE;

  return C37Result::OK;
}

C37Result deserialize_r2_pmu_config(std::span<const std::byte> message,
                                    std::uint16_t &offset, C37Pmu &pmu) {
  uint16_t format;

  if (auto result = deserialize_dynamic_string(message, offset, pmu.name);
      result != C37Result::OK)
    return result;

  if (auto result = deserialize(message, offset, pmu.idcode);
      result != C37Result::OK)
    return result;

  if (auto result = deserialize_uuid(message, offset, pmu.guid);
      result != C37Result::OK)
    return result;

  uint16_t phasor_count;
  uint16_t analog_count;
  uint16_t digital_count;
  if (auto result = deserialize(message, offset, format, phasor_count,
                                analog_count, digital_count);
      result != C37Result::OK)
    return result;

  pmu.freq_is_float = bit_field<3>(format);
  pmu.analog_is_float = bit_field<2>(format);
  pmu.phasor_is_float = bit_field<1>(format);
  pmu.phasor_is_polar = bit_field<0>(format);
  pmu.phasor.resize(phasor_count);
  pmu.analog.resize(analog_count);
  pmu.digital.resize(digital_count);

  for (auto &phasor : pmu.phasor) {
    if (auto result = deserialize_dynamic_string(message, offset, phasor.name);
        result != C37Result::OK)
      return result;
  }

  for (auto &analog : pmu.analog) {
    if (auto result = deserialize_dynamic_string(message, offset, analog.name);
        result != C37Result::OK)
      return result;
  }

  for (auto &digital : pmu.digital) {
    for (auto &name : digital.name) {
      if (auto result = deserialize_dynamic_string(message, offset, name);
          result != C37Result::OK)
        return result;
    }
  }

  for (auto &phasor : pmu.phasor) {
    if (auto result = deserialize_r2_phasor_scale(message, offset, phasor);
        result != C37Result::OK)
      return result;
  }

  for (auto &analog : pmu.analog) {
    if (auto result = deserialize_r2_analog_scale(message, offset, analog);
        result != C37Result::OK)
      return result;
  }

  for (auto &digital : pmu.digital) {
    uint32_t dgunit;
    if (auto result = deserialize(message, offset, dgunit);
        result != C37Result::OK)
      return result;

    digital.normal = bit_field<31, 16>(dgunit);
    digital.mask = bit_field<15, 0>(dgunit);
  }

  if (auto result = deserialize(message, offset, pmu.latitude, pmu.longitude,
                                pmu.elevation, pmu.service_class, pmu.window,
                                pmu.group_delay);
      result != C37Result::OK)
    return result;

  uint16_t nominal_frequency_flags;
  if (auto result =
          deserialize(message, offset, nominal_frequency_flags, pmu.cfgcnt);
      result != C37Result::OK)
    return result;

  pmu.nominal_frequency = bit_field<0>(nominal_frequency_flags) ? 50.0f : 60.0f;

  return C37Result::OK;
}

C37Result deserialize_r1_config(std::span<const std::byte> message,
                                C37Config &config) {
  uint16_t offset = 0;
  uint16_t pmu_count;

  if (auto result = deserialize(message, offset, config.time_base, pmu_count);
      result != C37Result::OK)
    return c37_result_requested_size(result) ? C37Result::INVALID_FRAMESIZE
                                             : result;

  config.pmu.resize(pmu_count);
  for (auto &pmu : config.pmu) {
    if (auto result = deserialize_r1_pmu_config(message, offset, pmu);
        result != C37Result::OK)
      return c37_result_requested_size(result) ? C37Result::INVALID_FRAMESIZE
                                               : result;
  }

  if (auto result = deserialize(message, offset, config.data_rate);
      result != C37Result::OK)
    return c37_result_requested_size(result) ? C37Result::INVALID_FRAMESIZE
                                             : result;

  return C37Result::OK;
}

C37Result deserialize_r2_config(std::span<const std::byte> message,
                                C37Config &config) {
  uint16_t offset = 0;
  uint16_t pmu_count;

  if (auto result = deserialize(message, offset, config.time_base, pmu_count);
      result != C37Result::OK)
    return result;

  config.pmu.resize(pmu_count);
  for (auto &pmu : config.pmu) {
    if (auto result = deserialize_r2_pmu_config(message, offset, pmu);
        result != C37Result::OK)
      return result;
  }

  if (auto result = deserialize(message, offset, config.data_rate);
      result != C37Result::OK)
    return result;

  return C37Result::OK;
}

template <typename T> void to_be_bytes_unchecked(std::byte *buffer, T value) {
  if constexpr (std::is_integral_v<T>) {
    using U = std::make_unsigned_t<T>;
    auto u = static_cast<U>(value);
    for (auto i : std::views::iota(size_t(0), sizeof(T)) | std::views::reverse)
      *buffer++ = std::byte((u >> (i * 8)) & 0xFF);
  } else if constexpr (std::is_same_v<T, float>) {
    static_assert(sizeof(float) == sizeof(uint32_t));
    to_be_bytes_unchecked(buffer, std::bit_cast<uint32_t>(value));
  } else {
    to_be_bytes_unchecked(buffer,
                          static_cast<std::underlying_type_t<T>>(value));
  }
}

template <typename... T>
  requires(... and (std::is_integral_v<T> or std::is_same_v<T, float> or
                    std::is_enum_v<T>))
void serialize(std::vector<std::byte> &buffer, T... value) {
  auto offset = buffer.size();
  buffer.resize(offset + (... + sizeof(T)));
  ((to_be_bytes_unchecked(buffer.data() + offset, value), offset += sizeof(T)),
   ...);
}

void serialize_fixed_string(std::vector<std::byte> &buffer,
                            std::string_view fixed_string) {
  auto bytes = reinterpret_cast<std::byte const *>(fixed_string.data());
  auto count = std::min({fixed_string.size(), size_t(16)});
  auto iter = buffer.insert(buffer.end(), 16, std::byte(' '));
  std::copy_n(bytes, count, iter);
}

void serialize_dynamic_string(std::vector<std::byte> &buffer,
                              std::string_view dynamic_string) {
  auto bytes = reinterpret_cast<std::byte const *>(dynamic_string.data());
  auto count = std::min({dynamic_string.size(), size_t(255)});
  serialize(buffer, uint8_t(count));
  buffer.insert(buffer.end(), bytes, bytes + count);
}

void serialize_phasor(std::vector<std::byte> &buffer, C37Pmu const &pmu,
                      C37PhasorInfo const &info, std::complex<float> phasor) {
  if (pmu.phasor_is_float) {
    if (pmu.phasor_is_polar) {
      serialize(buffer, std::abs(phasor), std::arg(phasor));
    } else {
      serialize(buffer, phasor.real(), phasor.imag());
    }
  } else {
    if (pmu.phasor_is_polar) {
      auto mag = std::bit_cast<int16_t>(
          uint16_t(std::abs(phasor) / info.amplitude_scale));
      auto ang = int16_t((std::arg(phasor) + info.phase_shift) * 10'000);
      serialize(buffer, mag, ang);
    } else {
      if (info.phase_shift != 0)
        phasor *= std::polar<float>(1, info.phase_shift);

      auto real = int16_t(phasor.real() / info.amplitude_scale);
      auto imag = int16_t(phasor.imag() / info.amplitude_scale);
      serialize(buffer, real, imag);
    }
  }
}

void serialize_freq(std::vector<std::byte> &buffer, C37Pmu const &pmu,
                    float freq) {
  if (pmu.freq_is_float) {
    serialize(buffer, freq);
  } else {
    serialize(buffer, uint16_t((freq - pmu.nominal_frequency) * 1'000));
  }
}

void serialize_dfreq(std::vector<std::byte> &buffer, C37Pmu const &pmu,
                     float dfreq) {
  if (pmu.freq_is_float) {
    serialize(buffer, dfreq);
  } else {
    serialize(buffer, uint16_t(dfreq * 100));
  }
}

void serialize_analog(std::vector<std::byte> &buffer, C37Pmu const &pmu,
                      C37AnalogInfo const &info, float analog) {
  if (pmu.analog_is_float) {
    serialize(buffer, analog);
  } else {
    serialize(buffer, uint16_t(info.scale != 0 ? analog / info.scale : 0.0f));
  }
}

void serialize_digital(std::vector<std::byte> &buffer, uint16_t digital) {
  serialize(buffer, digital);
}

void serialize_pmu_data(std::vector<std::byte> &buffer, C37Pmu const &pmu,
                        C37Data const &data) {
  uint16_t stat = 0;
  bit_field<13>(stat) = uint16_t(data.data_error);
  bit_field<12>(stat) = uint16_t(data.out_of_sync);
  bit_field<11>(stat) = uint16_t(data.sorted_by_arrival);
  bit_field<10>(stat) = uint16_t(data.trigger_detected);
  bit_field<9>(stat) = uint16_t(data.configuration_change);
  bit_field<8, 6>(stat) = uint16_t(data.time_error);
  bit_field<5, 4>(stat) = uint16_t(data.unlocked_time);
  bit_field<3, 0>(stat) = uint16_t(data.trigger_reason);
  serialize(buffer, stat);

  for (auto const i : std::views::iota(size_t(0), pmu.phasor.size()))
    serialize_phasor(buffer, pmu, pmu.phasor[i], data.phasor[i]);

  serialize_freq(buffer, pmu, data.freq);
  serialize_dfreq(buffer, pmu, data.dfreq);

  for (auto const i : std::views::iota(size_t(0), pmu.analog.size()))
    serialize_analog(buffer, pmu, pmu.analog[i], data.analog[i]);

  for (auto const i : std::views::iota(size_t(0), pmu.digital.size()))
    serialize_digital(buffer, data.digital[i]);
}

void serialize_r1_pmu_config(std::vector<std::byte> &buffer,
                             C37Pmu const &pmu) {
  serialize_fixed_string(buffer, pmu.name);

  uint16_t format = 0;
  bit_field<3>(format) = uint16_t(pmu.freq_is_float);
  bit_field<2>(format) = uint16_t(pmu.analog_is_float);
  bit_field<1>(format) = uint16_t(pmu.phasor_is_float);
  bit_field<0>(format) = uint16_t(pmu.phasor_is_polar);

  serialize(buffer, pmu.idcode, format, uint16_t(pmu.phasor.size()),
            uint16_t(pmu.analog.size()), uint16_t(pmu.digital.size()));

  for (auto const &phasor : pmu.phasor)
    serialize_fixed_string(buffer, phasor.name);
  for (auto const &analog : pmu.analog)
    serialize_fixed_string(buffer, analog.name);
  for (auto const &digital : pmu.digital)
    for (auto const &name : digital.name)
      serialize_fixed_string(buffer, name);

  for (auto const &phasor : pmu.phasor) {
    uint32_t phunit = 0;
    bit_field<31, 24>(phunit) = uint32_t(phasor.unit);
    bit_field<23, 0>(phunit) = uint32_t(phasor.amplitude_scale);
    serialize(buffer, phunit);
  }

  for (auto const &analog : pmu.analog) {
    uint32_t anunit = 0;
    bit_field<31, 24>(anunit) = uint32_t(analog.unit);
    bit_field<23, 0>(anunit) = uint32_t(analog.scale);
    serialize(buffer, anunit);
  }

  for (auto const &digital : pmu.digital) {
    uint32_t dgunit = 0;
    bit_field<31, 16>(dgunit) = uint32_t(digital.normal);
    bit_field<15, 0>(dgunit) = uint32_t(digital.mask);
    serialize(buffer, dgunit);
  }

  uint16_t fnom = pmu.nominal_frequency == 50.0f ? 1 : 0;
  serialize(buffer, fnom, pmu.cfgcnt);
}

void serialize_uuid(std::vector<std::byte> &buffer, uuid_t const &uuid) {
  auto bytes = reinterpret_cast<std::byte const *>(uuid);
  buffer.insert(buffer.end(), bytes, bytes + sizeof(uuid_t));
}

void serialize_r2_phasor_scale(std::vector<std::byte> &buffer,
                               C37PhasorInfo const &phasor) {
  uint32_t flags = 0;
  bit_field<31>(flags) = uint32_t(phasor.other_modification);
  bit_field<26>(flags) = uint32_t(phasor.pseudo_phasor_value);
  bit_field<25>(flags) = uint32_t(phasor.phase_adjusted_for_rotation);
  bit_field<24>(flags) = uint32_t(phasor.phase_adjusted_for_calibration);
  bit_field<23>(flags) = uint32_t(phasor.magnitude_adjusted_for_calibration);
  bit_field<22>(flags) = uint32_t(phasor.filtered_without_resampling);
  bit_field<21>(flags) = uint32_t(phasor.downsampled_with_non_fir_filter);
  bit_field<20>(flags) = uint32_t(phasor.downsampled_with_fir_filter);
  bit_field<19>(flags) = uint32_t(phasor.downsampled_with_reselection);
  bit_field<18>(flags) = uint32_t(phasor.upsampled_with_extrapolation);
  bit_field<17>(flags) = uint32_t(phasor.upsampled_with_interpolation);
  bit_field<11>(flags) = uint32_t(phasor.unit);
  bit_field<10, 8>(flags) = uint32_t(phasor.component);

  serialize(buffer, flags, phasor.amplitude_scale, phasor.phase_shift);
}

void serialize_r2_analog_scale(std::vector<std::byte> &buffer,
                               C37AnalogInfo const &analog) {
  serialize(buffer, analog.scale, analog.offset);
}

void serialize_r2_pmu_metadata(std::vector<std::byte> &buffer,
                               C37Pmu const &pmu) {
  serialize(buffer, pmu.latitude, pmu.longitude, pmu.elevation,
            pmu.service_class, pmu.window, pmu.group_delay);
}

void serialize_r2_pmu_config(std::vector<std::byte> &buffer,
                             C37Pmu const &pmu) {
  serialize_dynamic_string(buffer, pmu.name);
  serialize(buffer, pmu.idcode);
  serialize_uuid(buffer, pmu.guid);

  uint16_t format = 0;
  bit_field<3>(format) = uint16_t(pmu.freq_is_float);
  bit_field<2>(format) = uint16_t(pmu.analog_is_float);
  bit_field<1>(format) = uint16_t(pmu.phasor_is_float);
  bit_field<0>(format) = uint16_t(pmu.phasor_is_polar);

  serialize(buffer, format, uint16_t(pmu.phasor.size()),
            uint16_t(pmu.analog.size()), uint16_t(pmu.digital.size()));

  for (auto const &phasor : pmu.phasor)
    serialize_dynamic_string(buffer, phasor.name);
  for (auto const &analog : pmu.analog)
    serialize_dynamic_string(buffer, analog.name);
  for (auto const &digital : pmu.digital)
    for (auto const &name : digital.name)
      serialize_dynamic_string(buffer, name);

  for (auto const &phasor : pmu.phasor)
    serialize_r2_phasor_scale(buffer, phasor);
  for (auto const &analog : pmu.analog)
    serialize_r2_analog_scale(buffer, analog);

  for (auto const &digital : pmu.digital) {
    uint32_t dgunit = 0;
    bit_field<31, 16>(dgunit) = uint32_t(digital.normal);
    bit_field<15, 0>(dgunit) = uint32_t(digital.mask);
    serialize(buffer, dgunit);
  }

  serialize_r2_pmu_metadata(buffer, pmu);

  uint16_t fnom = pmu.nominal_frequency == 50.0f ? 1 : 0;
  serialize(buffer, fnom, pmu.cfgcnt);
}

std::size_t serialize_frame_header(std::vector<std::byte> &buffer, C37Sync sync,
                                   C37FrameMetadata const &metadata,
                                   C37Config const &config) {
  auto frame_start = buffer.size();

  uint32_t fracsec =
      config.time_base != 0
          ? uint32_t((uint64_t(metadata.soc.tv_nsec) * config.time_base +
                      500'000'000) /
                     1'000'000'000)
          : 0;

  uint32_t fracsec_bits = 0;
  bit_field<30>(fracsec_bits) =
      static_cast<uint32_t>(metadata.leap_second_direction);
  bit_field<29>(fracsec_bits) =
      static_cast<uint32_t>(metadata.leap_second_occurred);
  bit_field<28>(fracsec_bits) =
      static_cast<uint32_t>(metadata.leap_second_pending);
  bit_field<27, 24>(fracsec_bits) =
      static_cast<uint32_t>(metadata.message_time_quality);
  bit_field<23, 0>(fracsec_bits) = fracsec;

  serialize(buffer, sync,
            /* placeholder framesize */ uint16_t(0), metadata.idcode,
            uint32_t(metadata.soc.tv_sec), fracsec_bits);

  return frame_start;
}

void finalize_frame(std::vector<std::byte> &buffer, std::size_t frame_start) {
  auto total_size = uint16_t(buffer.size() - frame_start + sizeof(uint16_t));

  to_be_bytes_unchecked(buffer.data() + frame_start + 2, total_size);

  auto frame_data = std::span<const std::byte>(buffer).subspan(frame_start);
  serialize(buffer, calculate_crc(frame_data));
}

C37Result serialize_r1_config(C37Sync sync, C37FrameMetadata const &metadata,
                              C37Config const &config,
                              std::vector<std::byte> &buffer) {
  buffer.clear();

  auto frame_start = serialize_frame_header(buffer, sync, metadata, config);
  serialize(buffer, config.time_base, uint16_t(config.pmu.size()));

  for (auto const &pmu : config.pmu)
    serialize_r1_pmu_config(buffer, pmu);

  serialize(buffer, config.data_rate);
  finalize_frame(buffer, frame_start);

  return C37Result::OK;
}

void serialize_r2_config(C37Config const &config,
                         std::vector<std::byte> &buffer) {
  serialize(buffer, config.time_base, uint16_t(config.pmu.size()));

  for (auto const &pmu : config.pmu)
    serialize_r2_pmu_config(buffer, pmu);

  serialize(buffer, config.data_rate);
}

} // namespace

uint16_t C37Frame::size() const { return size_; }

C37Sync C37Frame::sync() const { return sync_; }

C37FrameMetadata C37Frame::metadata(C37Config const &config) const {
  auto soc_nanos = config.time_base != 0
                       ? uint64_t(fracsec_) * 1'000'000'000 / config.time_base
                       : 0;

  return {.idcode = idcode_,
          .soc = {.tv_sec = soc_, .tv_nsec = uint32_t(soc_nanos)},
          .leap_second_direction = leap_second_direction_,
          .leap_second_occurred = bool(leap_second_occurred_),
          .leap_second_pending = bool(leap_second_pending_),
          .message_time_quality = message_time_quality_};
}

C37Result C37Frame::load(std::span<const std::byte> buffer) {
  std::uint16_t offset = 0;

  uint32_t fracsec_bits;
  uint16_t crc;

  if (auto result = deserialize(buffer, offset, sync_, size_, idcode_, soc_,
                                fracsec_bits);
      result != C37Result::OK)
    return result;

  // A frame cannot be smaller than the size of the mandatory header and the trailing CRC.
  if (size_ < offset + sizeof(crc))
    return C37Result::INVALID_FRAMESIZE;

  leap_second_direction_ =
      bit_field<30>(fracsec_bits).as<C37LeapSecondDirection>();
  leap_second_occurred_ = bit_field<29>(fracsec_bits);
  leap_second_pending_ = bit_field<28>(fracsec_bits);
  message_time_quality_ =
      bit_field<27, 24>(fracsec_bits).as<C37MessageTimeQuality>();
  fracsec_ = bit_field<23, 0>(fracsec_bits);

  auto message_offset = offset;
  auto message_size = size_ - offset - sizeof(crc);

  offset += message_size;
  if (auto result = deserialize(buffer, offset, crc); result != C37Result::OK)
    return result;

  if (crc != calculate_crc(buffer.subspan(0, size_ - sizeof(crc))))
    return C37Result::INVALID_CHECKSUM;

  message_ = buffer.subspan(message_offset, message_size);

  return C37Result::OK;
}

C37Result C37Frame::deserialize_data(C37Config const &config,
                                     std::vector<C37Data> &data) const {
  assert(sync_ == C37Sync::R1_DATA);

  std::uint16_t offset = 0;

  data.resize(config.pmu.size());
  for (auto const i : std::views::iota(size_t(0), config.pmu.size())) {
    if (auto result =
            deserialize_pmu_data(message_, config.pmu[i], offset, data[i]);
        result != C37Result::OK) {
      return c37_result_requested_size(result) ? C37Result::INVALID_FRAMESIZE
                                               : result;
    }
  }

  return C37Result::OK;
}

C37Result C37Frame::deserialize_header(std::string_view &header) const {
  assert(sync_ == C37Sync::R1_HEADER);

  auto data = reinterpret_cast<char const *>(message_.data());
  header = std::string_view(data, message_.size());

  return C37Result::OK;
}

C37Result C37Frame::deserialize_config(
    C37Config &config, std::vector<std::byte> &segmentation_buffer) const {
  switch (sync_) {
  case C37Sync::R1_CONFIG1:
  case C37Sync::R1_CONFIG2: {
    auto result = deserialize_r1_config(message_, config);
    return c37_result_requested_size(result) ? C37Result::INVALID_FRAMESIZE
                                             : result;
  }

  case C37Sync::R2_CONFIG3: {
    uint16_t offset = 0;
    uint16_t segment;

    if (auto result = deserialize(message_, offset, segment);
        result != C37Result::OK)
      return c37_result_requested_size(result) ? C37Result::INVALID_FRAMESIZE
                                               : result;

    auto message = message_.subspan(offset);
    if (segment == 0) {
      auto result = deserialize_r2_config(message, config);
      return c37_result_requested_size(result) ? C37Result::INVALID_FRAMESIZE
                                               : result;
    }

    if (segment == 1)
      segmentation_buffer.clear();

    segmentation_buffer.insert(segmentation_buffer.end(), message.begin(),
                               message.end());

    if (segment != 0xFFFF)
      return C37Result::SEGMENTED;

    auto result = deserialize_r2_config(segmentation_buffer, config);
    return c37_result_requested_size(result) ? C37Result::INVALID_FRAMESIZE
                                             : result;
  }

  default:
    throw std::runtime_error("unreachable");
  }
}

C37Result C37Frame::deserialize_command(C37Command &command) const {
  assert(sync_ == C37Sync::R1_COMMAND);

  uint16_t offset = 0;

  if (auto result = deserialize(message_, offset, command.cmd);
      result != C37Result::OK)
    return c37_result_requested_size(result) ? C37Result::INVALID_FRAMESIZE
                                             : result;

  command.ext = message_.subspan(offset);

  return C37Result::OK;
}

C37Result C37Frame::serialize_data(std::span<const C37Data> data,
                                   C37FrameMetadata metadata,
                                   C37Config const &config,
                                   std::vector<std::byte> &buffer) {
  buffer.clear();
  auto frame_start =
      serialize_frame_header(buffer, C37Sync::R1_DATA, metadata, config);

  for (auto const i : std::views::iota(size_t(0), config.pmu.size()))
    serialize_pmu_data(buffer, config.pmu[i], data[i]);

  finalize_frame(buffer, frame_start);
  return C37Result::OK;
}

C37Result C37Frame::serialize_header(std::string_view header,
                                     C37FrameMetadata metadata,
                                     C37Config const &config,
                                     std::vector<std::byte> &buffer) {
  buffer.clear();
  auto frame_start =
      serialize_frame_header(buffer, C37Sync::R1_HEADER, metadata, config);

  auto const *data = reinterpret_cast<std::byte const *>(header.data());
  buffer.insert(buffer.end(), data, data + header.size());

  finalize_frame(buffer, frame_start);
  return C37Result::OK;
}

C37Result C37Frame::serialize_config(C37ConfigType type,
                                     C37FrameMetadata metadata,
                                     C37Config const &config,
                                     std::vector<std::byte> &buffer) {
  buffer.clear();

  switch (type) {
  case C37ConfigType::R1_CONFIG1:
    return serialize_r1_config(C37Sync::R1_CONFIG1, metadata, config, buffer);
  case C37ConfigType::R1_CONFIG2:
    return serialize_r1_config(C37Sync::R1_CONFIG2, metadata, config, buffer);

  case C37ConfigType::R2_CONFIG3: {
    std::vector<std::byte> content;
    serialize_r2_config(config, content);

    auto content_iter = content.begin();
    for (uint16_t segment = 1; content_iter != content.end(); segment += 1) {
      auto frame_offset = buffer.size();
      serialize_frame_header(buffer, C37Sync::R2_CONFIG3, metadata, config);

      auto segment_size =
          std::min({size_t(content.end() - content_iter), size_t(0xFFED)});
      auto segment_end = content_iter + segment_size;

      if (segment_end == content.end()) {
        segment = (segment == 1) ? 0 : 0xFFFF;
      }

      serialize(buffer, segment);
      buffer.insert(buffer.end(), content_iter, segment_end);
      finalize_frame(buffer, frame_offset);

      content_iter = segment_end;
    }

    return C37Result::OK;
  }

  default:
    throw std::runtime_error("unreachable");
  }
}

C37Result C37Frame::serialize_command(C37Command const &command,
                                      C37FrameMetadata metadata,
                                      C37Config const &config,
                                      std::vector<std::byte> &buffer) {
  buffer.clear();
  auto frame_start =
      serialize_frame_header(buffer, C37Sync::R1_COMMAND, metadata, config);

  serialize(buffer, command.cmd);
  buffer.insert(buffer.end(), command.ext.begin(), command.ext.end());

  finalize_frame(buffer, frame_start);
  return C37Result::OK;
}

namespace {

// A frame buffer sized to the maximum possible frame. The C37.118 frame size
// is a 16-bit field, so a single frame never exceeds 64 KiB.
using C37Buffer = std::array<std::byte, 1 << 16>;

std::tuple<std::string, std::string>
parse_address(std::string const &address, std::string default_service) {
  auto colon = address.rfind(':');
  if (colon == std::string::npos)
    return {address, std::move(default_service)};

  auto host = address.substr(0, colon);
  auto service = address.substr(colon + 1);
  return {std::move(host), std::move(service)};
}

class C37Node : public Node {
  struct {
    std::string host;
    std::string service;
    uint16_t idcode = 1;
    int sd = -1;
    C37Config config = {};

    std::vector<std::byte> tx = {};
    C37Buffer rx = {};
  } client;

  struct {
    std::string host;
    std::string service;
    bool testing = false;
    uint16_t idcode = 1;
    C37Config config = {};

    int listen_sd = -1;
    int client_sd = -1;
    bool streaming = false;

    CQueueSignalled queue = {};
    std::jthread thread;

    std::vector<std::string> names = {};
    std::vector<int> indices = {};
    std::vector<C37Data> data = {};
    std::vector<std::byte> tx = {};
    C37Buffer rx = {};
  } server;

  std::string details = {};

  static void sendAll(int sd, std::span<const std::byte> data);
  static bool recvAll(int sd, std::span<std::byte> buf);
  static C37Result recvFrame(int sd, C37Buffer &buffer, C37Frame &frame);

  C37PhasorInfo parsePhasor(Json const &json,
                            std::vector<std::string> &names) const;
  C37AnalogInfo parseAnalog(Json const &json,
                            std::vector<std::string> &names) const;
  void parseDigital(Json const &json, std::vector<C37DigitalInfo> &digital,
                    std::vector<std::string> &names) const;
  C37Pmu parsePmu(Json const &json, std::vector<std::string> &names) const;
  C37Config parseConfig(Json const &json,
                        std::vector<std::string> &names) const;

  void clientConnect();
  void clientDisconnect();
  void clientSendCommand(C37CommandType cmd);
  void clientBuildSignals();

  void serverListen();
  void serverShutdown();
  void serverLoop(std::stop_token stop);
  void serverAcceptClient();
  void serverCloseClient();
  void serverHandleCommand();
  void serverDrainQueue();
  void serverBuildIndices(SignalList &signals);
  void serverSendSample(struct Sample const *smp);

protected:
  int _read(struct Sample *smps[], unsigned cnt) override;
  int _write(struct Sample *smps[], unsigned cnt) override;

public:
  using Node::Node;

  ~C37Node() override {
    clientDisconnect();
    serverShutdown();
  }

  int parse(json_t *json) override;
  int prepare() override;
  int start() override;
  int stop() override;
  int reverse() override;
  const std::string &getDetails() override;

  std::vector<int> getPollFDs() override {
    return client.sd >= 0 ? std::vector<int>{client.sd} : std::vector<int>{};
  }
};

void C37Node::clientConnect() {
  addrinfo hints = {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  auto addr = std::invoke([&]() {
    addrinfo *result = nullptr;

    if (auto ret = getaddrinfo(client.host.c_str(), client.service.c_str(),
                               &hints, &result);
        ret != 0)
      throw RuntimeError("Failed to resolve '{}:{}': {}", client.host,
                         client.service, gai_strerror(ret));

    return std::unique_ptr<addrinfo, decltype([](addrinfo *ptr) {
                             freeaddrinfo(ptr);
                           })>(result);
  });

  int fd = -1;
  for (auto *rp = addr.get(); rp != nullptr; rp = rp->ai_next) {
    fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd < 0)
      continue;

    if (::connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
      break;

    ::close(fd);
    fd = -1;
  }

  if (fd < 0)
    throw SystemError("Failed to connect to '{}:{}'", client.host,
                      client.service);

  client.sd = fd;
}

void C37Node::clientDisconnect() {
  if (client.sd < 0)
    return;

  ::close(client.sd);
  client.sd = -1;
}

void C37Node::sendAll(int sd, std::span<const std::byte> data) {
  std::size_t offset = 0;
  while (offset < data.size()) {
    auto sent =
        ::send(sd, data.data() + offset, data.size() - offset, MSG_NOSIGNAL);
    if (sent < 0)
      throw SystemError("Failed to send C37.118 frame");

    offset += sent;
  }
}

bool C37Node::recvAll(int sd, std::span<std::byte> buf) {
  std::size_t offset = 0;
  while (offset < buf.size()) {
    auto received = ::recv(sd, buf.data() + offset, buf.size() - offset, 0);
    if (received < 0)
      throw SystemError("Failed to receive from C37.118 connection");

    if (received == 0)
      return false;

    offset += received;
  }

  return true;
}

C37Result C37Node::recvFrame(int sd, C37Buffer &buffer, C37Frame &frame) {
  for (std::size_t count = 0;;) {
    auto result = frame.load(std::span(buffer).first(count));

    auto required = c37_result_requested_size(result);
    if (required == 0)
      return result;

    if (!recvAll(sd, std::span(buffer).subspan(count, required - count)))
      throw RuntimeError("Connection closed by peer");

    count = required;
  }
}

void C37Node::clientSendCommand(C37CommandType cmd) {
  C37Command command = {.cmd = cmd, .ext = {}};

  C37FrameMetadata metadata = {
      .idcode = client.idcode,
      .soc = time_now(),
  };

  C37Frame::serialize_command(command, metadata, client.config, client.tx);

  sendAll(client.sd, client.tx);
}

void C37Node::clientBuildSignals() {
  auto signals = std::make_shared<SignalList>();

  for (auto const i : std::views::iota(size_t(0), client.config.pmu.size())) {
    auto const &pmu = client.config.pmu[i];

    // Prefix every signal with the PMU index so the names are unique across
    // PMUs and usable in path signal-mapping expressions.
    for (auto const &phasor : pmu.phasor)
      signals->push_back(std::make_shared<Signal>(
          fmt::format("pmu{}_{}", i, phasor.name), "", SignalType::COMPLEX));

    signals->push_back(std::make_shared<Signal>(
        fmt::format("pmu{}_frequency", i), "Hz", SignalType::FLOAT));
    signals->push_back(std::make_shared<Signal>(fmt::format("pmu{}_rocof", i),
                                                "Hz/s", SignalType::FLOAT));

    for (auto const &analog : pmu.analog)
      signals->push_back(std::make_shared<Signal>(
          fmt::format("pmu{}_{}", i, analog.name), "", SignalType::FLOAT));

    for (auto const &digital : pmu.digital)
      for (auto const b : std::views::iota(size_t(0), digital.name.size()))
        if ((digital.mask >> b) & 1)
          signals->push_back(std::make_shared<Signal>(
              fmt::format("pmu{}_{}", i, digital.name[b]), "",
              SignalType::BOOLEAN));
  }

  in.signals = signals;
}

C37PhasorInfo C37Node::parsePhasor(Json const &json,
                                   std::vector<std::string> &names) const {
  C37PhasorInfo phasor;
  auto const &signal = names.emplace_back(json.at("signal"));
  phasor.name = json.value("name", signal);

  phasor.unit = json.at("unit").get<C37PhasorUnit>();
  phasor.component = json.value("component", C37PhasorComponent::PHASE_A);
  phasor.amplitude_scale = 1.0f;
  phasor.phase_shift = 0.0f;

  phasor.upsampled_with_interpolation = false;
  phasor.upsampled_with_extrapolation = false;
  phasor.downsampled_with_reselection = false;
  phasor.downsampled_with_fir_filter = false;
  phasor.downsampled_with_non_fir_filter = false;
  phasor.filtered_without_resampling = false;
  phasor.magnitude_adjusted_for_calibration = false;
  phasor.phase_adjusted_for_calibration = false;
  phasor.phase_adjusted_for_rotation = false;
  phasor.pseudo_phasor_value = false;
  phasor.other_modification = false;

  if (json.contains("modifications")) {
    for (auto const &json_mod : json.at("modifications")) {
      auto const &mod = json_mod.get_ref<Json::string_t const &>();

      if (mod == "upsampled_with_interpolation")
        phasor.upsampled_with_interpolation = true;
      else if (mod == "upsampled_with_extrapolation")
        phasor.upsampled_with_extrapolation = true;
      else if (mod == "downsampled_with_reselection")
        phasor.downsampled_with_reselection = true;
      else if (mod == "downsampled_with_fir_filter")
        phasor.downsampled_with_fir_filter = true;
      else if (mod == "downsampled_with_non_fir_filter")
        phasor.downsampled_with_non_fir_filter = true;
      else if (mod == "filtered_without_resampling")
        phasor.filtered_without_resampling = true;
      else if (mod == "magnitude_adjusted_for_calibration")
        phasor.magnitude_adjusted_for_calibration = true;
      else if (mod == "phase_adjusted_for_calibration")
        phasor.phase_adjusted_for_calibration = true;
      else if (mod == "phase_adjusted_for_rotation")
        phasor.phase_adjusted_for_rotation = true;
      else if (mod == "pseudo_phasor_value")
        phasor.pseudo_phasor_value = true;
      else if (mod == "other")
        phasor.other_modification = true;
      else
        throw RuntimeError("Invalid phasor modification '{}'", mod);
    }
  }

  return phasor;
}

C37AnalogInfo C37Node::parseAnalog(Json const &json,
                                   std::vector<std::string> &names) const {
  C37AnalogInfo analog;
  auto const &signal = names.emplace_back(json.at("signal"));
  analog.name = json.value("name", signal);

  analog.unit = json.value("unit", C37AnalogUnit::POINT_ON_WAVE);
  analog.scale = 1.0f;
  analog.offset = 0.0f;

  return analog;
}

void C37Node::parseDigital(Json const &json,
                           std::vector<C37DigitalInfo> &digital,
                           std::vector<std::string> &names) const {
  // Start a new status word when there is none yet or the last one is full.
  if (digital.empty() || digital.back().mask == 0xFFFF)
    digital.emplace_back();

  auto &word = digital.back();
  auto const b = std::countr_one(word.mask);

  auto const &signal = names.emplace_back(json.at("signal"));
  word.name[b] = json.value("name", signal);
  word.mask |= uint16_t(1 << b);
  if (json.value("normal", false))
    word.normal |= uint16_t(1 << b);
}

C37Pmu C37Node::parsePmu(Json const &json,
                         std::vector<std::string> &names) const {
  C37Pmu pmu;
  pmu.name = json.at("name");
  pmu.idcode = json.value("idcode", 1);

  if (json.contains("guid")) {
    if (uuid_parse(json.at("guid").get_ref<Json::string_t const &>().c_str(),
                   pmu.guid) != 0)
      throw RuntimeError("Invalid PMU guid '{}'", json.at("guid"));
  } else {
    uuid_copy(pmu.guid, uuid);
  }

  pmu.phasor_is_polar = false;
  pmu.phasor_is_float = true;
  pmu.analog_is_float = true;
  pmu.freq_is_float = true;

  pmu.phasor.clear();
  if (json.contains("phasor"))
    for (auto const &phasor : json.at("phasor"))
      pmu.phasor.push_back(parsePhasor(phasor, names));

  names.emplace_back(json.at("frequency"));
  names.emplace_back(json.at("rocof"));

  pmu.analog.clear();
  if (json.contains("analog"))
    for (auto const &analog : json.at("analog"))
      pmu.analog.push_back(parseAnalog(analog, names));

  // Each entry of the digital array is a single bit, batched into 16-bit words.
  pmu.digital.clear();
  if (json.contains("digital"))
    for (auto const &digital : json.at("digital"))
      parseDigital(digital, pmu.digital, names);

  pmu.latitude =
      json.value("latitude", std::numeric_limits<float>::quiet_NaN());
  pmu.longitude =
      json.value("longitude", std::numeric_limits<float>::quiet_NaN());
  pmu.elevation =
      json.value("elevation", std::numeric_limits<float>::quiet_NaN());
  pmu.service_class = json.value("service_class", C37ServiceClass::M);
  pmu.window = json.value("window", 0);
  pmu.group_delay = json.value("group_delay", 0);

  pmu.nominal_frequency = json.value("nominal_frequency", 50.0f);
  if (pmu.nominal_frequency != 50.0f && pmu.nominal_frequency != 60.0f)
    throw RuntimeError("Invalid nominal frequency {} Hz; must be 50 or 60 Hz",
                       pmu.nominal_frequency);

  pmu.cfgcnt = 0;

  return pmu;
}

C37Config C37Node::parseConfig(Json const &json,
                               std::vector<std::string> &names) const {
  C37Config config;
  config.time_base = json.value("time_base", 1'000'000);

  config.pmu.clear();
  if (json.contains("pmus"))
    for (auto const &pmu : json.at("pmus"))
      config.pmu.push_back(parsePmu(pmu, names));

  // The reporting rate is configured in frames per second. C37.118 encodes it
  // as a signed integer: a positive value is frames per second, a negative
  // value is seconds per frame.
  auto data_rate = json.at("data_rate").get<double>();
  if (data_rate <= 0.0)
    throw RuntimeError("Invalid data rate {}; must be positive", data_rate);

  config.data_rate = data_rate >= 1.0 ? int16_t(std::lround(data_rate))
                                      : int16_t(-std::lround(1.0 / data_rate));

  return config;
}

int C37Node::parse(json_t *json) {
  if (auto ret = Node::parse(json); ret)
    return ret;

  Json config = json;

  if (auto in = config.find("in");
      in != config.end() && in->contains("address")) {
    auto address = in->at("address").get<std::string>();
    client.idcode = in->value("idcode", client.idcode);
    std::tie(client.host, client.service) = parse_address(address, "4712");
  }

  if (auto out = config.find("out");
      out != config.end() && out->contains("address")) {
    auto address = out->at("address").get<std::string>();
    server.idcode = out->value("idcode", server.idcode);
    server.testing = out->value("testing", server.testing);

    // The configuration properties are inlined into the "out" object.
    server.names.clear();
    server.config = parseConfig(*out, server.names);

    std::tie(server.host, server.service) = parse_address(address, "4712");
  }

  return 0;
}

int C37Node::prepare() {
  if (out.enabled) {
    serverListen();

    if (auto ret = queue_signalled_init(&server.queue); ret)
      throw RuntimeError("Failed to initialize server queue");

    server.thread = std::jthread(
        [this](std::stop_token stop) { serverLoop(std::move(stop)); });
  }

  if (in.enabled) {
    clientConnect();

    clientSendCommand(C37CommandType::R1_GET_CONFIG2);

    C37Frame frame;
    std::vector<std::byte> segmentation_buffer;
    for (;;) {
      if (auto result = recvFrame(client.sd, client.rx, frame);
          result != C37Result::OK)
        throw RuntimeError("Failed to load frame ({})", int(result));

      auto sync = frame.sync();
      if (sync != C37Sync::R1_CONFIG2 && sync != C37Sync::R2_CONFIG3) {
        logger->debug("Ignoring frame type {:#x}", int(sync));
        continue;
      }

      auto result =
          frame.deserialize_config(client.config, segmentation_buffer);
      if (result == C37Result::OK)
        break;

      if (result == C37Result::SEGMENTED)
        continue;

      throw RuntimeError("Failed to deserialize configuration frame ({})",
                         int(result));
    }

    clientBuildSignals();

    logger->info("Received configuration with {} PMU(s) and {} signal(s)",
                 client.config.pmu.size(), in.signals->size());
  }

  return Node::prepare();
}

int C37Node::start() {
  if (auto ret = Node::start(); ret)
    return ret;

  if (in.enabled)
    clientSendCommand(C37CommandType::R1_DATA_START);

  if (out.enabled)
    serverBuildIndices(*getOutputSignals());

  return 0;
}

int C37Node::stop() {
  if (in.enabled) {
    clientSendCommand(C37CommandType::R1_DATA_STOP);
    clientDisconnect();
  }

  if (out.enabled)
    serverShutdown();

  return Node::stop();
}

int C37Node::reverse() {
  // We can turn a server into a client connecting to its own address, but there
  // is no meaningful way to turn a client into a server.
  if (in.enabled)
    throw RuntimeError("Cannot reverse a C37.118 client into a server");

  client.host = server.host;
  client.service = server.service;
  client.idcode = server.idcode;

  std::swap(in.enabled, out.enabled);

  return 0;
}

const std::string &C37Node::getDetails() {
  details.clear();

  auto append = [&](std::string const &entry) {
    if (!details.empty())
      details += ", ";
    details += entry;
  };

  if (in.enabled) {
    append(fmt::format("in.address={}:{}", client.host, client.service));
    for (auto const i : std::views::iota(size_t(0), client.config.pmu.size()))
      append(fmt::format("in.pmu[{}].name={}", i, client.config.pmu[i].name));
  }

  if (out.enabled) {
    append(fmt::format("out.address={}:{}", server.host, server.service));
    for (auto const i : std::views::iota(size_t(0), server.config.pmu.size()))
      append(fmt::format("out.pmu[{}].name={}", i, server.config.pmu[i].name));
  }

  return details;
}

int C37Node::_read(struct Sample *smps[], unsigned cnt) {
  C37Frame frame;

  if (auto result = recvFrame(client.sd, client.rx, frame);
      result != C37Result::OK) {
    logger->warn("Dropping invalid frame ({})", int(result));
    return 0;
  }

  if (frame.sync() != C37Sync::R1_DATA) {
    logger->debug("Ignoring non-data frame of type {:#x}", int(frame.sync()));
    return 0;
  }

  std::vector<C37Data> data;
  if (auto result = frame.deserialize_data(client.config, data);
      result != C37Result::OK) {
    logger->warn("Failed to deserialize data frame ({})", int(result));
    return 0;
  }

  auto metadata = frame.metadata(client.config);

  auto *smp = smps[0];
  smp->length = 0;
  smp->signals = getInputSignals();
  smp->flags = (int)SampleFlags::HAS_TS_ORIGIN | (int)SampleFlags::HAS_DATA;
  smp->ts.origin = metadata.soc;

  auto push = [&](SignalData value) {
    if (smp->length < smp->capacity)
      smp->data[smp->length++] = value;
  };

  for (auto const i : std::views::iota(size_t(0), data.size())) {
    auto const &d = data[i];
    auto const &pmu = client.config.pmu[i];

    for (auto const &phasor : d.phasor)
      push(SignalData(phasor));

    push(SignalData(d.freq));
    push(SignalData(d.dfreq));

    for (auto const &analog : d.analog)
      push(SignalData(analog));

    for (auto const j : std::views::iota(size_t(0), d.digital.size())) {
      auto const word = d.digital[j];
      auto const &info = pmu.digital[j];

      for (auto const b : std::views::iota(size_t(0), info.name.size()))
        if ((info.mask >> b) & 1)
          push(SignalData(bool((word >> b) & 1)));
    }
  }

  return 1;
}

void C37Node::serverListen() {
  addrinfo hints = {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  auto addr = std::invoke([&]() {
    addrinfo *result = nullptr;

    // An empty host binds to all local interfaces (AI_PASSIVE).
    auto host = server.host.empty() ? nullptr : server.host.c_str();

    if (auto ret = getaddrinfo(host, server.service.c_str(), &hints, &result);
        ret != 0)
      throw RuntimeError("Failed to resolve local address '{}:{}': {}",
                         server.host, server.service, gai_strerror(ret));

    return std::unique_ptr<addrinfo, decltype([](addrinfo *ptr) {
                             freeaddrinfo(ptr);
                           })>(result);
  });

  int fd = -1;
  for (auto *rp = addr.get(); rp != nullptr; rp = rp->ai_next) {
    fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd < 0)
      continue;

    int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    if (::bind(fd, rp->ai_addr, rp->ai_addrlen) == 0)
      break;

    ::close(fd);
    fd = -1;
  }

  if (fd < 0)
    throw SystemError("Failed to bind to local address '{}:{}'", server.host,
                      server.service);

  if (::listen(fd, 1) < 0) {
    ::close(fd);
    throw SystemError("Failed to listen on local address '{}:{}'", server.host,
                      server.service);
  }

  server.listen_sd = fd;
}

void C37Node::serverShutdown() {
  // A non-joinable thread means the server was never started (or was already
  // shut down), so there is nothing to tear down.
  if (!server.thread.joinable())
    return;

  server.thread.request_stop();
  server.thread.join();

  serverCloseClient();

  if (server.listen_sd >= 0) {
    ::close(server.listen_sd);
    server.listen_sd = -1;
  }

  // Release references to any samples still queued (e.g. if the loop exited on
  // an error before draining).
  void *ptr;
  while (queue_pull(&server.queue.queue, &ptr) == 1)
    sample_decref(static_cast<struct Sample *>(ptr));

  std::ignore = queue_signalled_destroy(&server.queue);
}

void C37Node::serverLoop(std::stop_token stop) {
  while (!stop.stop_requested() ||
         (server.streaming && queue_signalled_available(&server.queue) > 0)) {
    auto queue_fd = queue_signalled_fd(&server.queue);

    std::array<pollfd, 3> fds = {{
        {.fd = server.listen_sd, .events = POLLIN, .revents = 0},
        {.fd = queue_fd, .events = POLLIN, .revents = 0},
        {.fd = server.client_sd, .events = POLLIN, .revents = 0},
    }};

    auto ret = ::poll(fds.data(), server.client_sd != -1 ? 3 : 2, 100);
    if (ret < 0) {
      if (errno == EINTR)
        continue;

      logger->warn("Server poll failed: {}", strerror(errno));
      break;
    }

    if (fds[0].revents & POLLIN)
      serverAcceptClient();

    if (fds[1].revents & POLLIN)
      serverDrainQueue();

    if (fds[2].revents & (POLLIN | POLLHUP | POLLERR)) {
      try {
        serverHandleCommand();
      } catch (std::exception &e) {
        logger->warn("Client connection error: {}", e.what());
        serverCloseClient();
      }
    }
  }
}

void C37Node::serverAcceptClient() {
  int fd = ::accept(server.listen_sd, nullptr, nullptr);
  if (fd < 0) {
    logger->warn("Failed to accept connection: {}", strerror(errno));
    return;
  }

  if (server.client_sd >= 0) {
    logger->info("Replacing existing client connection");
    serverCloseClient();
  }

  server.client_sd = fd;
  server.streaming = false;
  logger->info("Client connected");
}

void C37Node::serverCloseClient() {
  if (server.client_sd < 0)
    return;

  ::close(server.client_sd);
  server.client_sd = -1;
  server.streaming = false;
}

void C37Node::serverHandleCommand() {
  C37Frame frame;
  if (auto result = recvFrame(server.client_sd, server.rx, frame);
      result != C37Result::OK) {
    logger->warn("Dropping invalid frame ({})", int(result));
    return;
  }

  if (frame.sync() != C37Sync::R1_COMMAND) {
    logger->debug("Ignoring non-command frame with sync {:#x}",
                  int(frame.sync()));
    return;
  }

  C37Command command;
  if (auto result = frame.deserialize_command(command);
      result != C37Result::OK) {
    logger->warn("Failed to deserialize command ({})", int(result));
    return;
  }

  C37FrameMetadata metadata = {
      .idcode = server.idcode,
      .soc = time_now(),
  };

  switch (command.cmd) {
  case C37CommandType::R1_GET_CONFIG1:
    C37Frame::serialize_config(C37ConfigType::R1_CONFIG1, metadata,
                               server.config, server.tx);
    sendAll(server.client_sd, server.tx);
    break;

  case C37CommandType::R1_GET_CONFIG2:
    C37Frame::serialize_config(C37ConfigType::R1_CONFIG2, metadata,
                               server.config, server.tx);
    sendAll(server.client_sd, server.tx);
    break;

  case C37CommandType::R2_GET_CONFIG3:
    C37Frame::serialize_config(C37ConfigType::R2_CONFIG3, metadata,
                               server.config, server.tx);
    sendAll(server.client_sd, server.tx);
    break;

  case C37CommandType::R1_DATA_START:
    server.streaming = true;
    logger->info("Client started data transmission");
    break;

  case C37CommandType::R1_DATA_STOP:
    server.streaming = false;
    logger->info("Client stopped data transmission");
    break;

  default:
    logger->warn("Ignoring unsupported command {:#x}", int(command.cmd));
  }
}

void C37Node::serverDrainQueue() {
  void *ptr;

  if (server.testing && !server.streaming)
    return;

  int ret;
  do {
    uint64_t cntr;
    ret = ::read(queue_signalled_fd(&server.queue), &cntr, sizeof(cntr));
  } while (ret < 0 && errno == EINTR);

  while (queue_signalled_available(&server.queue) != 0 and
         queue_signalled_pull(&server.queue, &ptr) == 1) {
    auto *smp = static_cast<struct Sample *>(ptr);

    try {
      if (server.streaming)
        serverSendSample(smp);
    } catch (std::exception &e) {
      logger->warn("Failed to send data frame: {}", e.what());
      serverCloseClient();
    }

    sample_decref(smp);
  }
}

void C37Node::serverBuildIndices(SignalList &signals) {
  server.indices.clear();
  server.indices.reserve(server.names.size());

  auto push = [&, name = server.names.begin()](SignalType expected) mutable {
    auto const &signal_name = *name++;

    auto index = signals.getIndexByName(signal_name);
    if (index < 0)
      throw RuntimeError("Configured signal '{}' not found in the input",
                         signal_name);

    auto signal = signals.getByIndex(index);
    if (signal->type != expected)
      throw RuntimeError(
          "Configured signal '{}' has type {} but {} is required", signal_name,
          signalTypeToString(signal->type), signalTypeToString(expected));

    server.indices.push_back(index);
  };

  for (auto const &pmu : server.config.pmu) {
    for (auto const &phasor : pmu.phasor) {
      std::ignore = phasor;
      push(SignalType::COMPLEX);
    }

    push(SignalType::FLOAT); // freq
    push(SignalType::FLOAT); // rocof

    for (auto const &analog : pmu.analog) {
      std::ignore = analog;
      push(SignalType::FLOAT);
    }

    for (auto const &word : pmu.digital)
      for (auto const b : std::views::iota(size_t(0), word.name.size()))
        if ((word.mask >> b) & 1)
          push(SignalType::BOOLEAN);
  }
}

void C37Node::serverSendSample(struct Sample const *smp) {
  auto const &config = server.config;
  auto &data = server.data;

  // Reuse the C37Data (and its per-PMU vectors) across samples; after the first
  // sample these resizes are no-ops and perform no allocation.
  data.resize(config.pmu.size());

  auto value = [&](int index) -> SignalData {
    return index >= 0 && unsigned(index) < smp->length ? smp->data[index]
                                                       : SignalData{};
  };

  auto index = server.indices.begin();
  auto next = [&]() -> SignalData { return value(*index++); };

  for (auto const i : std::views::iota(size_t(0), config.pmu.size())) {
    auto const &pmu = config.pmu[i];
    auto &d = data[i];

    d.trigger_reason = C37DataTriggerReason::MANUAL;
    d.unlocked_time =
        C37DataUnlockedTime::LOCKED_OR_UNLOCKED_FOR_LESS_THAN_10_SECONDS;
    d.time_error = C37DataTimeError::ESTIMATED_LESS_THAN_100_NANOSECONDS;
    d.configuration_change = false;
    d.trigger_detected = false;
    d.sorted_by_arrival = false;
    d.out_of_sync = false;
    d.data_error = false;

    d.phasor.resize(pmu.phasor.size());
    d.analog.resize(pmu.analog.size());
    d.digital.resize(pmu.digital.size());

    for (auto &phasor : d.phasor)
      phasor = next().z;

    d.freq = next().f;
    d.dfreq = next().f;

    for (auto &analog : d.analog)
      analog = next().f;

    for (auto const j : std::views::iota(size_t(0), pmu.digital.size())) {
      auto const &info = pmu.digital[j];

      uint16_t word = 0;
      for (auto const b : std::views::iota(size_t(0), info.name.size()))
        if ((info.mask >> b) & 1)
          if (next().b)
            word |= uint16_t(1 << b);

      d.digital[j] = word;
    }
  }

  C37FrameMetadata metadata = {
      .idcode = server.idcode,
      .soc = (smp->flags & (int)SampleFlags::HAS_TS_ORIGIN) ? smp->ts.origin
                                                            : time_now(),
  };

  C37Frame::serialize_data(data, metadata, config, server.tx);
  sendAll(server.client_sd, server.tx);
}

int C37Node::_write(struct Sample *smps[], unsigned cnt) {
  if (!server.thread.joinable())
    return -1;

  for (auto const i : std::views::iota(unsigned(0), cnt)) {
    sample_incref(smps[i]);

    if (queue_signalled_push(&server.queue, smps[i]) != 1) {
      sample_decref(smps[i]);
      logger->warn("Server queue overrun; dropping sample");
    }
  }

  return cnt;
}

} // namespace

static char name[] = "c37.118";
static char description[] = "IEEE C37.118 Synchrophasor protocol";
static NodePlugin<C37Node, name, description,
                  (int)NodeFactory::Flags::SUPPORTS_READ |
                      (int)NodeFactory::Flags::SUPPORTS_WRITE |
                      (int)NodeFactory::Flags::SUPPORTS_POLL |
                      (int)NodeFactory::Flags::PROVIDES_SIGNALS,
                  1>
    p;

} // namespace villas::node
