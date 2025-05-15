/* Node-type for exec node-types.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/format.hpp>
#include <villas/node.hpp>
#include <villas/popen.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class ExecNode : public Node {

protected:
  std::unique_ptr<villas::utils::Popen> proc;
  std::unique_ptr<Format> formatter;

  FILE *stream_in, *stream_out;

  bool flush;
  bool shell;

  std::string working_dir;
  std::string command;

  villas::utils::Popen::arg_list arguments;
  villas::utils::Popen::env_map environment;

  int _read(struct Sample *smps[], unsigned cnt) override;
  int _write(struct Sample *smps[], unsigned cnt) override;

public:
  ExecNode(const uuid_t &id = {}, const std::string &name = "")
      : Node(id, name), stream_in(nullptr), stream_out(nullptr), flush(true),
        shell(false) {}

  virtual ~ExecNode();

  const std::string &getDetails() override;

  int start() override;

  int stop() override;

  int prepare() override;

  int parse(json_t *json) override;

  std::vector<int> getPollFDs() override;
};

} // namespace node
} // namespace villas
