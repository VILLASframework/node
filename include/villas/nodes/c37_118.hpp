/* Node type: C37-118.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2024 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <variant>
#include <vector>
#include <villas/node.hpp>
#include <villas/node/config.hpp>
#include <villas/nodes/c37_118/parser.hpp>
#include <villas/queue_signalled.h>
#include <villas/signal.hpp>
#include <villas/socket_addr.hpp>
#include <villas/timing.hpp>

namespace villas::node::c37_118 {

using parser::Parser;
using types::Frame;
using types::Config;

class C37_118 final : public Node {
private:
  struct Server {
    std::string addr;
    uint16_t port;
    int socktype;
    Config config;

    int listener_fd;
    std::vector<int> connection_fds;
  };

  struct Client {
    std::string addr;
    uint16_t port;
    uint16_t idcode;
    int socktype;

    std::optional<Config> config;
    int connection_fd;
    std::vector<unsigned char> recv_buf = std::vector<unsigned char>(1024);
    Parser parser;

    void send(Frame::Variant message, timespec ts = time_now());
    std::optional<Frame> recv();
  };

  std::variant<Server, Client> role;

  void parseServer(json_t *json);
  void parseClient(json_t *json);

  void prepareServer(Server &server);
  void prepareClient(Client &client);

  virtual int _read(struct Sample *smps[], unsigned cnt) override;

public:
  C37_118(const uuid_t &id = {}, const std::string &name = "") : Node{id, name} {}

  virtual ~C37_118() override;
  virtual int parse(json_t *json) override;
  virtual int prepare() override;
  virtual int start() override;
  virtual int stop() override;
  virtual std::vector<int> getPollFDs() override;
};

} // namespace villas::node::c37_118
