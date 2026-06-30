/* Unit tests for C37.118 parser.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <bit>
#include <ranges>
#include <span>

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <villas/nodes/c37_118.hpp>

using namespace villas::node;

// cppcheck-suppress syntaxError
ParameterizedTestParameters(c37_118, parser) {
  static criterion::parameters<criterion::parameters<unsigned char>> params =
      {};

  params.push_back( // Config2
      {0xaa, 0x31, 0x00, 0x86, 0x00, 0xf1, 0x48, 0x93, 0x34, 0x4a, 0x00, 0x19,
       0x99, 0x9a, 0x00, 0xff, 0xff, 0xff, 0x00, 0x01, 0x42, 0x6c, 0x75, 0x65,
       0x20, 0x50, 0x4d, 0x55, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
       0x00, 0xf1, 0x00, 0x06, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x56, 0x31,
       0x4c, 0x50, 0x4d, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
       0x20, 0x20, 0x56, 0x41, 0x4c, 0x50, 0x4d, 0x20, 0x20, 0x20, 0x20, 0x20,
       0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x56, 0x42, 0x4c, 0x50, 0x4d, 0x20,
       0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x56, 0x43,
       0x4c, 0x50, 0x4d, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
       0x20, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
       0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x59, 0x00, 0x32,
       0xc1, 0xe2});

  params.push_back( // Data
      {0xaa, 0x01, 0x00, 0x36, 0x00, 0xf1, 0x48, 0x93, 0x34, 0x4a, 0x00,
       0x1e, 0xb8, 0x52, 0x08, 0x00, 0x42, 0xf6, 0x8f, 0x24, 0xc7, 0xc3,
       0x66, 0x23, 0x43, 0x01, 0x88, 0xcb, 0xc7, 0xc3, 0x63, 0x32, 0xc7,
       0xa9, 0x56, 0x76, 0x47, 0x42, 0xfe, 0x4b, 0x47, 0xa9, 0x1c, 0xdd,
       0x47, 0x43, 0xd1, 0x44, 0x00, 0x00, 0x00, 0x00, 0x47, 0xef});

  params.push_back( // Command
      {0xaa, 0x41, 0x00, 0x12, 0x00, 0xf1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x02, 0xa7, 0x37});

  return params;
}

ParameterizedTest(criterion::parameters<unsigned char> *param, c37_118,
                  parser) {
  static C37Config const config = {
      .time_base = 0x00FFFFFFu,
      .pmu = {C37Pmu{
          .name = {"Blue PMU"},
          .idcode = 241,
          .phasor_is_polar = false,
          .phasor_is_float = true,
          .analog_is_float = true,
          .freq_is_float = false,
          .phasor =
              {
                  C37PhasorInfo{.name = {"V1LPM"},
                                .unit = C37PhasorUnit::VOLT,
                                .amplitude_scale = 1},
                  C37PhasorInfo{.name = {"VALPM"},
                                .unit = C37PhasorUnit::VOLT,
                                .amplitude_scale = 1},
                  C37PhasorInfo{.name = {"VBLPM"},
                                .unit = C37PhasorUnit::VOLT,
                                .amplitude_scale = 1},
                  C37PhasorInfo{.name = {"VCLPM"},
                                .unit = C37PhasorUnit::VOLT,
                                .amplitude_scale = 1},
              },
          .analog = {},
          .digital = {},
          .nominal_frequency = 50.0f,
          .cfgcnt = 0x59,
      }},
      .data_rate = 50,
  };

  C37Frame frame;
  cr_assert_eq(
      frame.load(std::span(reinterpret_cast<std::byte *>(param->data()),
                           param->size())),
      C37Result::OK);

  switch (frame.sync()) {
  case C37Sync::R1_CONFIG2: {
    C37Config parsed;
    std::vector<std::byte> segmentation_buffer;
    cr_assert_eq(frame.deserialize_config(parsed, segmentation_buffer),
                 C37Result::OK);
    cr_assert_eq(parsed.time_base, config.time_base);
    cr_assert_eq(parsed.data_rate, config.data_rate);
    cr_assert_eq(parsed.pmu.size(), config.pmu.size());

    auto const &pmu = parsed.pmu[0];
    auto const &ref = config.pmu[0];
    cr_assert_str_eq(pmu.name.c_str(), ref.name.c_str());
    cr_assert_eq(pmu.idcode, ref.idcode);
    cr_assert_eq(pmu.phasor_is_float, ref.phasor_is_float);
    cr_assert_eq(pmu.phasor_is_polar, ref.phasor_is_polar);
    cr_assert_eq(pmu.analog_is_float, ref.analog_is_float);
    cr_assert_eq(pmu.freq_is_float, ref.freq_is_float);
    cr_assert_eq(pmu.nominal_frequency, ref.nominal_frequency);
    cr_assert_eq(pmu.cfgcnt, ref.cfgcnt);
    cr_assert_eq(pmu.phasor.size(), ref.phasor.size());
    cr_assert_eq(pmu.analog.size(), ref.analog.size());
    cr_assert_eq(pmu.digital.size(), ref.digital.size());
    for (auto const i : std::views::iota(size_t(0), ref.phasor.size())) {
      cr_assert_str_eq(pmu.phasor[i].name.c_str(), ref.phasor[i].name.c_str());
      cr_assert_eq(pmu.phasor[i].unit, ref.phasor[i].unit);
      cr_assert_eq(pmu.phasor[i].amplitude_scale,
                   ref.phasor[i].amplitude_scale);
    }

    std::vector<std::byte> out;
    cr_assert_eq(C37Frame::serialize_config(C37ConfigType::R1_CONFIG2,
                                            frame.metadata(parsed), parsed,
                                            out),
                 C37Result::OK);
    C37Frame frame2;
    cr_assert_eq(frame2.load(out), C37Result::OK);
    C37Config config2;
    cr_assert_eq(frame2.deserialize_config(config2, segmentation_buffer),
                 C37Result::OK);
    cr_assert_eq(config2.time_base, parsed.time_base);
    cr_assert_eq(config2.data_rate, parsed.data_rate);
    cr_assert_eq(config2.pmu[0].nominal_frequency, pmu.nominal_frequency);
    cr_assert_eq(config2.pmu[0].cfgcnt, pmu.cfgcnt);
  } break;

  case C37Sync::R1_DATA: {
    std::vector<C37Data> data;
    cr_assert_eq(frame.deserialize_data(config, data), C37Result::OK);
    cr_assert_eq(data.size(), 1u);

    auto const &d = data[0];
    cr_assert_eq(d.sorted_by_arrival, 1);
    cr_assert_eq(d.trigger_reason, C37DataTriggerReason::MANUAL);
    cr_assert_eq(d.data_error, false);
    cr_assert_eq(d.phasor.size(), 4u);
    cr_assert_eq(d.phasor[0].real(), std::bit_cast<float>(0x42f68f24u));
    cr_assert_eq(d.phasor[0].imag(), std::bit_cast<float>(0xc7c36623u));
    cr_assert_eq(d.phasor[1].real(), std::bit_cast<float>(0x430188cbu));
    cr_assert_eq(d.phasor[1].imag(), std::bit_cast<float>(0xc7c36332u));
    cr_assert_eq(d.phasor[2].real(), std::bit_cast<float>(0xc7a95676u));
    cr_assert_eq(d.phasor[2].imag(), std::bit_cast<float>(0x4742fe4bu));
    cr_assert_eq(d.phasor[3].real(), std::bit_cast<float>(0x47a91cddu));
    cr_assert_eq(d.phasor[3].imag(), std::bit_cast<float>(0x4743d144u));
    cr_assert_float_eq(d.freq, 50.0f, 1e-3f);
    cr_assert_float_eq(d.dfreq, 0.0f, 1e-6f);

    std::vector<std::byte> out;
    cr_assert_eq(
        C37Frame::serialize_data(data, frame.metadata(config), config, out),
        C37Result::OK);
    cr_assert_eq(out.size(), param->size());
    for (auto const index : std::views::iota(size_t(0), out.size()))
      cr_assert_eq(out[index], std::byte(param->at(index)),
                   "mismatch at index %lu", index);
  } break;

  case C37Sync::R1_COMMAND: {
    C37Command command;
    cr_assert_eq(frame.deserialize_command(command), C37Result::OK);
    cr_assert_eq(command.cmd, C37CommandType::R1_DATA_START);
    cr_assert_eq(command.ext.size(), 0u);

    std::vector<std::byte> out;
    cr_assert_eq(C37Frame::serialize_command(
                     {.cmd = C37CommandType::R1_DATA_START, .ext = {}},
                     frame.metadata(config), config, out),
                 C37Result::OK);
    cr_assert_eq(out.size(), param->size());
    cr_assert_eq(std::memcmp(out.data(), param->data(), param->size()), 0);
  } break;

  default:
    break;
  }
}
