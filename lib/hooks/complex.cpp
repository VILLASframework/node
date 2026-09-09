/* polar_to_rect hook.
 *
 * Author: Alexandra Bach <alexandra.bach@eonerc.rwth-aachen.de>, Jason Chrysoprase <jason.chrysoprase@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cassert>
#include <cmath>
#include <string>
#include <jansson.h>

#include <villas/hook.hpp>
#include <villas/sample.hpp>
#include <villas/node/exceptions.hpp>
#include "villas/signal.hpp"

namespace villas {
namespace node {

class PolarToRectHook : public MultiSignalHook {

protected:
  std::string angle;
  char *signal_name;
  unsigned signal_index;
  bool angle_in_degrees = false;
  int polar_to_rect;

public:
  PolarToRectHook(Path *p, Node *n, int fl, int prio)
      : MultiSignalHook(p, n, fl, prio), angle("deg"), polar_to_rect(0) {}

  void parse(json_t *json) override {
    int ret;
    json_error_t err;
    const char *angle = nullptr;

    assert(state != State::STARTED);

    MultiSignalHook::parse(json);

    ret = json_unpack_ex(json, &err, 0, "{ s: s, s: b }", "angle", &angle, "polar_to_rect", &polar_to_rect);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-polar_to_rect");

    if (std::strcmp(angle, "deg") == 0)
      angle_in_degrees = true;
    else if (std::strcmp(angle, "rad") == 0)
      angle_in_degrees = false;
    else
      throw ConfigError(json, "node-config-hook-polar_to_rect",
                        "'angle' must be either 'deg' or 'rad'");

    if (polar_to_rect != 0 && polar_to_rect != 1)
      throw ConfigError(json, "node-config-hook-polar_to_rect",
                        "'polar_to_rect' must be either 0 or 1");

    logger->debug("PolarToRectHook: Parsed configuration with angle unit: {}, polar_to_rect: {}", angle_in_degrees ? "degrees" : "radians", polar_to_rect);

    state = State::PARSED;
  }

  void check() override {
    state = State::CHECKED;
  }

  void prepare() override {
    logger->debug("Preparing polar_to_rect hook");
    assert(state == State::CHECKED);

    std::string new_sig_name, new_sig_name2;

    MultiSignalHook::prepare();

    assert(state != State::STARTED);

    // logger->debug("PolarToRectHook: Using signal index {}", signal_index);

    signal_index = 0;
    logger->debug("Preparing polar_to_rect hook with {} signals", signalIndices.size());
    if (polar_to_rect == true){
      if (signalIndices.size() == 2) {
        auto orig_sig = signals->getByIndex(signal_index);

        if (!orig_sig)
          throw RuntimeError("PolarToRectHook: Failed to find signal");

        if (orig_sig->type == SignalType::COMPLEX)
          throw RuntimeError("Signal is complex");

        new_sig_name = fmt::format("{}_rect", orig_sig->name);

        auto new_sig = std::make_shared<Signal>(new_sig_name, orig_sig->unit,
                                                SignalType::COMPLEX);
        if (!new_sig)
          throw RuntimeError("Failed to create new signal");

        // delte old signals, assuming that they are behind each other
        signals->erase(signals->begin());
        signals->erase(signals->begin());

        // insert new signal
        signals->insert(signals->end(), new_sig);
        logger->debug("Inserted new signal at index {}", signal_index);
      }
    } else {
      logger->debug("Preparing polar_to_rect hook with 1 signal and polar_to_rect=false: index {}", signal_index);
      auto orig_sig = signals->getByIndex(signal_index);

      if (!orig_sig)
        throw RuntimeError("PolarToRectHook: Failed to find signal");

      if (orig_sig->type != SignalType::COMPLEX)
        throw RuntimeError("Signal is not complex");

      if (signalIndices.size() != 1)
        throw RuntimeError("Hook 'polar_to_rect' requires either 1 or 2 signals, but {} were provided", signalIndices.size());

      new_sig_name = fmt::format("{}_mag", orig_sig->name);
      new_sig_name2 = fmt::format("{}_angle", orig_sig->name);

      auto new_sig = std::make_shared<Signal>(new_sig_name, orig_sig->unit,
                                              SignalType::FLOAT);
      auto new_sig2 = std::make_shared<Signal>(new_sig_name2, orig_sig->unit,
                                              SignalType::FLOAT);
      logger->debug("Creating new signals: {} and {}", new_sig_name, new_sig_name2);
      if (!new_sig || !new_sig2)
        throw RuntimeError("Failed to create new signal");

      // delte old signal at the very first position (signal_index = 0)
      signals->erase(signals->begin());

      // insert new signal
      signals->insert(signals->end(), new_sig);
      signals->insert(signals->end(), new_sig2);
    }

    state = State::PREPARED;
  }

  Hook::Reason process(struct Sample *smp) override {
    logger->debug("Processing polar_to_rect hook with {} signals", signalIndices.size());
    assert(state == State::STARTED);

    if (!smp || smp->length == 0)
      return Reason::OK;

    if (polar_to_rect == true){
      // if (smp->length % 2 != 0){
      //   logger->error("Hook 'polar_to_rect' requires an even number of samples, but {} were provided", smp->length);
      //   return Reason::ERROR;
      // }

      if  (signalIndices.size() == 1) {
        logger->debug("Processing polar_to_rect hook with 1 signal: index {}", signalIndices.front());
        size_t pair_count = smp->length / 2;

        for (size_t i = 0; i < pair_count; i++) {
          double magnitude = smp->data[2 * i];
          double angle = smp->data[2 * i + 1];

          if (angle_in_degrees)
            angle = angle * M_PI / 180.0;

          double real = magnitude * cos(angle);
          double imag = magnitude * sin(angle);

          smp->data[2 * i] = real;
          smp->data[2 * i + 1] = imag;
        }
      }
      else if (signalIndices.size() == 2) {
        logger->debug("Processing polar_to_rect hook with 2 signals: magnitude index {}, angle index {}", signalIndices.front(), signalIndices.back());
        double magnitude = smp->data[0].f;
        double angle = smp->data[1].f;

        if (angle_in_degrees)
          angle = angle * M_PI / 180.0;

        double real = magnitude * cos(angle);   // requires rad
        double imag = magnitude * sin(angle);

        std::complex<double> complex_value(real, imag);

        sample_data_remove(smp, 0, 1);
        sample_data_remove(smp, 0, 1);
        sample_data_insert(smp, reinterpret_cast<union SignalData *>(&complex_value), smp->length, 1);
        smp->data[smp->length-1].z = complex_value;

        logger->debug("After insert: smp->length = {}", smp->length);
        for (size_t i = 0; i < smp->length; i++) {
            logger->debug("smp->data[{}]: real {} and imag {}", i, smp->data[i].z.real(), smp->data[i].z.imag());
        }

        logger->debug("Signal inserted at index {}: real={}, imag={}", signal_index, complex_value.real(), complex_value.imag());
      }
      else {
        logger->error("Hook 'polar_to_rect' requires either 1 or 2 signals, but {} were provided", signalIndices.size());
        return Reason::ERROR;
      }
    } else {
      logger->debug("Length of sample: {}, signal index: {}, length of sampleIndices {}", smp->length, signal_index, signalIndices.size());

      if (signalIndices.size() != 1) {
        logger->error("Hook 'polar_to_rect' requires 1 signal, but {} were provided", signalIndices.size());
        return Reason::ERROR;
      }
      // logger->debug("Signalname {}", signals->getByIndex(0)->name);
      std::complex<double> complex_value = smp->data[0].z;
      logger->debug("Real and imag values: real={}, imag={}", complex_value.real(), complex_value.imag());

      double magnitude = std::abs(complex_value);
      double angle = std::arg(complex_value);

      if (angle_in_degrees)
        angle = angle * 180.0 / M_PI;

      logger->debug("Processing rect_to_polar hook: magnitude={}, angle={}, signal_index={}", magnitude, angle, signal_index);
      sample_data_remove(smp, signal_index, 1);
      sample_data_insert(smp, reinterpret_cast<union SignalData *>(&magnitude), smp->length, 1);
      sample_data_insert(smp, reinterpret_cast<union SignalData *>(&angle), smp->length, 1);
    }



    return Reason::OK;
  }
};

// Register hook
static char n[] = "polar_to_rect";
static char d[] = "Convert polar coordinates (magnitude, angle) to rectangular (real, imag)";

static HookPlugin<PolarToRectHook, n, d,
                  (int)Hook::Flags::PATH |
                  (int)Hook::Flags::NODE_READ |
                  (int)Hook::Flags::NODE_WRITE>
    p;

} // namespace node
} // namespace villas