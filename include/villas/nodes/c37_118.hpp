/* C37.118.2 PMU/PDC communication protocol types and parser.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <array>
#include <complex>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include <uuid.h>

namespace villas::node {

enum class C37Version : uint8_t {
  R1_2004,
  R2_2011,
};

/** format type enum for C37.118 protocol values */
enum class C37Format {
  FIXED,
  FLOATING_POINT,
};

/** unit type enum for C37.118 protocol phasors */
enum class C37PhasorUnit : uint32_t {
  VOLT,
  AMPERE,
};

/** phasor component represented by a C37.118 phasor */
enum class C37PhasorComponent : uint32_t {
  ZERO_SEQUENCE = 0b000,
  POSITIVE_SEQUENCE = 0b001,
  NEGATIVE_SEQUENCE = 0b010,
  PHASE_A = 0b100,
  PHASE_B = 0b101,
  PHASE_C = 0b110,
};

/** unit type enum for C37.118 protocol phasors */
enum class C37AnalogUnit : uint32_t {
  POINT_ON_WAVE,
  RMS,
  PEAK,
};

/** service class of a C37.118 PMU (measurement or protection).
 * The values match the ASCII characters used in the SVC_CLASS field. */
enum class C37ServiceClass : uint8_t {
  M = 'M',
  P = 'P',
};

enum class C37DataTriggerReason : uint16_t {
  MANUAL = 0x0,
  LOW_MAGNITUDE,
  HIGH_MAGNITUDE,
  PHASE_SHIFT,
  HIGH_FREQUENCY_DEVIATION,
  HIGH_ROCOF,
  DIGITAL = 0x7,
};

enum class C37DataUnlockedTime : uint16_t {
  LOCKED_OR_UNLOCKED_FOR_LESS_THAN_10_SECONDS,
  UNLOCKED_FOR_10S_TO_100_SECONDS,
  UNLOCKED_FOR_100S_TO_1000_SECONDS,
  UNLOCKED_FOR_MORE_THAN_1000_SECONDS,
};

enum class C37DataTimeError : uint16_t {
  NOT_USED,
  ESTIMATED_LESS_THAN_100_NANOSECONDS,
  ESTIMATED_LESS_THAN_1_MICROSECOND,
  ESTIMATED_LESS_THAN_10_MICROSECONDS,
  ESTIMATED_LESS_THAN_100_MICROSECONDS,
  ESTIMATED_LESS_THAN_1_MILLISECOND,
  ESTIMATED_LESS_THAN_10_MILLISECONDS,
  UNKNOWN_OR_ESTIMATED_MORE_THAN_100_MILLISECONDS,
};

/** data packet in C37.118 protocol frames */
struct C37Data {
  struct {
    C37DataTriggerReason trigger_reason : 4;
    C37DataUnlockedTime unlocked_time : 2;
    C37DataTimeError time_error : 3;
    bool configuration_change : 1;
    bool trigger_detected : 1;
    bool sorted_by_arrival : 1;
    bool out_of_sync : 1;
    bool data_error : 1;
  };

  std::vector<std::complex<float>> phasor;
  float freq;
  float dfreq;
  std::vector<float> analog;
  std::vector<uint16_t> digital;
};

/** metadata for C37.118 protocol phasors */
struct C37PhasorInfo {
  std::string name;
  C37PhasorUnit unit;
  C37PhasorComponent component;
  float amplitude_scale;
  float phase_shift;

  // Phasor measurement modifications (CONFIG3 PHSCALE flags), one bit each.
  struct {
    bool upsampled_with_interpolation : 1;       // bit 0
    bool upsampled_with_extrapolation : 1;       // bit 1
    bool downsampled_with_reselection : 1;       // bit 2
    bool downsampled_with_fir_filter : 1;        // bit 3
    bool downsampled_with_non_fir_filter : 1;    // bit 4
    bool filtered_without_resampling : 1;        // bit 5
    bool magnitude_adjusted_for_calibration : 1; // bit 6
    bool phase_adjusted_for_calibration : 1;     // bit 7
    bool phase_adjusted_for_rotation : 1;        // bit 8
    bool pseudo_phasor_value : 1;                // bit 9
    bool other_modification : 1;                 // modification-flags MSB
  };
};

/** metadata for C37.118 protocol analog values */
struct C37AnalogInfo {
  std::string name;
  C37AnalogUnit unit : 8;
  float scale;
  float offset;
};

/** metadata for C37.118 protocol digital values */
struct C37DigitalInfo {
  std::array<std::string, 16> name;
  uint16_t normal;
  uint16_t mask;
};

/** configuration for one C37.118 PMU */
struct C37Pmu {
  std::string name;
  uint16_t idcode;
  uuid_t guid;

  struct {
    bool phasor_is_polar : 1;
    bool phasor_is_float : 1;
    bool analog_is_float : 1;
    bool freq_is_float : 1;
  };

  std::vector<C37PhasorInfo> phasor;
  std::vector<C37AnalogInfo> analog;
  std::vector<C37DigitalInfo> digital;

  float latitude;
  float longitude;
  float elevation;
  C37ServiceClass service_class;
  uint32_t window;
  uint32_t group_delay;

  float nominal_frequency;

  uint16_t cfgcnt;
};

/** configuration for a C37.118 PMU or PDC containing one or more PMU configurations */
struct C37Config {
  uint32_t time_base;
  std::vector<C37Pmu> pmu;
  uint16_t data_rate;
};

enum class C37ConfigType {
  R1_CONFIG1,
  R1_CONFIG2,
  R2_CONFIG3,
};

enum class C37CommandType : uint16_t {
  R1_DATA_STOP = 1,
  R1_DATA_START,
  R1_GET_HEADER,
  R1_GET_CONFIG1,
  R1_GET_CONFIG2,
  R2_GET_CONFIG3,
};

struct C37Command {
  C37CommandType cmd;
  std::span<const std::byte> ext;
};

enum class C37Result : uint16_t {
  OK,
  SEGMENTED,
  INVALID_FRAMESIZE,
  INVALID_CHECKSUM,
  _requested_size_minimum = 14,
};

constexpr std::size_t c37_result_requested_size(C37Result result) {
  using underlying_type = std::underlying_type_t<C37Result>;

  constexpr auto minimum =
      static_cast<underlying_type>(C37Result::_requested_size_minimum);

  if (auto value = static_cast<underlying_type>(result); value >= minimum)
    return value;

  return 0;
}

enum class C37Sync : uint16_t {
  R1_DATA = 0xAA01,
  R1_HEADER = 0xAA11,
  R1_CONFIG1 = 0xAA21,
  R1_CONFIG2 = 0xAA31,
  R1_COMMAND = 0xAA41,
  R2_CONFIG3 = 0xAA52,
};

enum class C37LeapSecondDirection : uint32_t {
  ADD,
  DELETE,
};

/** time quality indicator for a C37.118 message */
enum class C37MessageTimeQuality : uint32_t {
  LOCKED_TO_UTC,
  WITHIN_1_NANOS_OF_UTC,
  WITHIN_10_NANOS_OF_UTC,
  WITHIN_100_NANOS_OF_UTC,
  WITHIN_1_MICROS_OF_UTC,
  WITHIN_10_MICROS_OF_UTC,
  WITHIN_100_MICROS_OF_UTC,
  WITHIN_1_MILLIS_OF_UTC,
  WITHIN_10_MILLIS_OF_UTC,
  WITHIN_100_MILLIS_OF_UTC,
  WITHIN_1_SECS_OF_UTC,
  WITHIN_10_SECS_OF_UTC,
  TIME_NOT_RELIABLE,
};

struct C37FrameMetadata {
  uint16_t idcode;
  timespec soc;
  C37LeapSecondDirection leap_second_direction = {};
  bool leap_second_occurred = false;
  bool leap_second_pending = false;
  C37MessageTimeQuality message_time_quality =
      C37MessageTimeQuality::LOCKED_TO_UTC;
};

class C37Frame {
  C37Sync sync_;
  uint16_t size_;
  uint16_t idcode_;
  uint32_t soc_;

  struct {
    uint32_t fracsec_;
    C37LeapSecondDirection leap_second_direction_ : 1;
    uint32_t leap_second_occurred_ : 1;
    uint32_t leap_second_pending_ : 1;
    C37MessageTimeQuality message_time_quality_ : 4;
  };

  std::span<const std::byte> message_;

public:
  C37Frame() = default;
  uint16_t size() const;
  C37Version version() const;
  C37Sync sync() const;
  C37FrameMetadata metadata(C37Config const &config) const;

  /** load a buffer into this frame */
  C37Result load(std::span<const std::byte> buffer);

  /** deserialize a data frame */
  C37Result deserialize_data(C37Config const &config,
                             std::vector<C37Data> &data) const;

  /** deserialize a header frame */
  C37Result deserialize_header(std::string_view &header) const;

  /** deserialize a configuration frame
   *
   * A CONFIG3 frame may be fragmented across several frames. The
   * segmentation_buffer accumulates the segments; the result is SEGMENTED until
   * the final segment completes the configuration. */
  C37Result
  deserialize_config(C37Config &config,
                     std::vector<std::byte> &segmentation_buffer) const;

  /** deserialize a command frame */
  C37Result deserialize_command(C37Command &command) const;

  /** serialize a data frame */
  static C37Result serialize_data(std::span<const C37Data> data,
                                  C37FrameMetadata metadata,
                                  C37Config const &config,
                                  std::vector<std::byte> &buffer);

  /** serialize a header frame */
  static C37Result serialize_header(std::string_view header,
                                    C37FrameMetadata metadata,
                                    C37Config const &config,
                                    std::vector<std::byte> &buffer);

  /** serialize a configuration frame */
  static C37Result serialize_config(C37ConfigType type,
                                    C37FrameMetadata metadata,
                                    C37Config const &config,
                                    std::vector<std::byte> &buffer);

  /** serialize a command frame */
  static C37Result serialize_command(C37Command const &command,
                                     C37FrameMetadata metadata,
                                     C37Config const &config,
                                     std::vector<std::byte> &buffer);
};

} // namespace villas::node
