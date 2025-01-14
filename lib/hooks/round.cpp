/* Round hook.
 *
 * Author: Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/hook.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class RoundHook : public MultiSignalHook {

protected:
  unsigned precision;

public:
  RoundHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : MultiSignalHook(p, n, fl, prio, en), precision(1) {}

  virtual void parse(json_t *json) {
    int ret;
    json_error_t err;

    assert(state != State::STARTED);

    MultiSignalHook::parse(json);

    ret = json_unpack_ex(json, &err, 0, "{ s?: i}", "precision", &precision);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-round");

    state = State::PARSED;
  }

  virtual Hook::Reason process(struct Sample *smp) {
    for (auto index : signalIndices) {
      assert(index < smp->length);
      assert(state == State::STARTED);

      switch (sample_format(smp, index)) {
      case SignalType::FLOAT:
        smp->data[index].f =
            round(smp->data[index].f * pow(10, precision)) / pow(10, precision);
        break;

      case SignalType::COMPLEX:
        smp->data[index].z = std::complex<float>(
            round(smp->data[index].z.real() * pow(10, precision)) /
                pow(10, precision),
            round(smp->data[index].z.imag() * pow(10, precision)) /
                pow(10, precision));
        break;
      default: {
      }
      }
    }

    return Reason::OK;
  }
};

// Register hook
static char n[] = "round";
static char d[] = "Round signals to a set number of digits";
static HookPlugin<RoundHook, n, d,
                  (int)Hook::Flags::PATH | (int)Hook::Flags::NODE_READ |
                      (int)Hook::Flags::NODE_WRITE>
    p;

} // namespace node
} // namespace villas
