/* Print hook.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdio>
#include <cstring>

#include <villas/format.hpp>
#include <villas/hook.hpp>
#include <villas/json.hpp>
#include <villas/node.hpp>
#include <villas/path.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class PrintHook : public Hook {

protected:
  Format::Ptr formatter;

  std::string prefix;
  std::string output_path;

  FILE *output;
  std::vector<char> output_buffer;

public:
  PrintHook(Path *p, Node *n, int fl, int prio)
      : Hook(p, n, fl, prio), output(nullptr) {}

  void start() override {
    assert(state == State::PREPARED || state == State::STOPPED);

    if (!output_path.empty()) {
      output = fopen(output_path.c_str(), "w+");
      if (!output)
        throw SystemError("Failed to open file");
    } else
      output_buffer = std::vector<char>(DEFAULT_FORMAT_BUFFER_LENGTH);

    formatter->start(signals);

    state = State::STARTED;
  }

  void stop() override {
    if (output)
      fclose(output);
  }

  void parse(json_t *json) override {
    const char *p = nullptr;
    const char *o = nullptr;
    int ret;
    json_error_t err;
    json_t *json_format = nullptr;

    assert(state == State::INITIALIZED || state == State::CHECKED ||
           state == State::PARSED);

    Hook::parse(json);

    ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: o, s?: s }", "prefix", &p,
                         "format", &json_format, "output", &o);
    if (ret)
      throw JsonError(""_json_pointer, "{}", err.text);

    if (p && o) {
      throw JsonError(""_json_pointer,
                      "Prefix and output settings are exclusive.");
    }

    if (p)
      prefix = p;

    if (o)
      output_path = o;

    // Format
    auto *fmt = json_format
                    ? JsonError::context(
                          "/format"_json_pointer,
                          [&] { return FormatFactory::make(json_format); })
                    : FormatFactory::make("villas.human");

    formatter = Format::Ptr(fmt);
    if (!formatter)
      throw JsonError("/format"_json_pointer, "Invalid format configuration");

    state = State::PARSED;
  }

  Hook::Reason process(struct Sample *smp) override {
    assert(state == State::STARTED);

    if (!output) {
      char *buf = output_buffer.data();
      size_t buflen = output_buffer.size();
      size_t wbytes;

      formatter->sprint(buf, buflen, &wbytes, smp);

      if (wbytes > 0 && buf[wbytes - 1] == '\n')
        buf[wbytes - 1] = 0;

      if (node)
        logger->info("{}{} {}", prefix, node->getName(), buf);
      else if (path)
        logger->info("{}{} {}", prefix, path->toString(), buf);
    } else
      formatter->print(output, smp);

    return Reason::OK;
  }
};

// Register hook
static char n[] = "print";
static char d[] = "Print the message to stdout or a file";
static HookPlugin<PrintHook, n, d,
                  (int)Hook::Flags::NODE_READ | (int)Hook::Flags::NODE_WRITE |
                      (int)Hook::Flags::PATH>
    p;

} // namespace node
} // namespace villas
