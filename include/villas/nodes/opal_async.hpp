/* Node type: OPAL-RT Asynchronous Process (libOpalAsyncApi)
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 * SPDX-FileCopyrightText: 2023-2025 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <condition_variable>
#include <mutex>

#include <spdlog/details/log_msg_buffer.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>

// Define RTLAB before including OpalPrint.h for messages to be sent
// to the OpalDisplay. Otherwise stdout will be used.
#define RTLAB

#include <AsyncApi.h>
#include <OpalGenAsyncParamCtrl.h>
#include <OpalPrint.h>

#include <villas/format.hpp>
#include <villas/node.hpp>
#include <villas/popen.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {
namespace opal {

class LogSink final : public spdlog::sinks::base_sink<std::mutex> {
private:
  std::string shmemSystemCtrlName;

public:
  explicit LogSink(const std::string &shmemName);

  ~LogSink() override;

protected:
  void sink_it_(const spdlog::details::log_msg &msg) override;

  void flush_() override {}
};

} // namespace opal

// Forward declarations
struct Sample;

class OpalAsyncNode : public Node {

protected:
  // RT-LAB -> VILLASnode
  // Corresponds to AsyncAPI's *Send* direction
  struct {
    unsigned id;
    bool present;
    unsigned length;

    bool reply;
    int mode;

    Opal_SendAsyncParam params;
  } in;

  // VILLASnode -> RT-LAB
  // Corresponds to AsyncAPI's *Recv* direction
  struct {
    unsigned id;
    bool present;
    unsigned length;

    Opal_RecvAsyncParam params;
  } out;

  bool ready;
  std::mutex readyLock;
  std::condition_variable readyCv;

  virtual int _read(struct Sample *smps[], unsigned cnt) override;

  virtual int _write(struct Sample *smps[], unsigned cnt) override;

public:
  OpalAsyncNode(const uuid_t &id = {}, const std::string &name = "")
      : Node(id, name), ready(false) {
    in.id = 1;
    in.present = false;
    in.length = 0;
    in.reply = true;

    out.id = 1;
    out.present = false;
    out.length = 0;
  }

  virtual const std::string &getDetails() override;

  virtual int start() override;
  virtual int stop() override;

  virtual int parse(json_t *json) override;

  void markReady();
  void waitReady();
};

class OpalAsyncNodeFactory : public NodeFactory {

public:
  using NodeFactory::NodeFactory;

  virtual Node *make(const uuid_t &id = {},
                     const std::string &nme = "") override {
    auto *n = new OpalAsyncNode(id, nme);

    init(n);

    return n;
  }

  virtual int getFlags() const override {
    return (int)NodeFactory::Flags::SUPPORTS_READ |
           (int)NodeFactory::Flags::SUPPORTS_WRITE |
           (int)NodeFactory::Flags::PROVIDES_SIGNALS;
  }

  virtual std::string getName() const override { return "opal.async"; }

  virtual std::string getDescription() const override {
    return "OPAL Asynchronous Process (libOpalAsyncApi)";
  }

  virtual int getVectorize() const override { return 1; }

  virtual int start(SuperNode *sn) override;

  virtual int stop() override;
};

} // namespace node
} // namespace villas
