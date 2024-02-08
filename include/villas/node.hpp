/* Nodes.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fmt/ostream.h>
#include <iostream>
#include <jansson.h>
#include <uuid/uuid.h>
#include <villas/colors.hpp>
#include <villas/common.hpp>
#include <villas/list.hpp>
#include <villas/log.hpp>
#include <villas/node/memory.hpp>
#include <villas/node_direction.hpp>
#include <villas/node_list.hpp>
#include <villas/path_destination.hpp>
#include <villas/path_source.hpp>
#include <villas/plugin.hpp>
#include <villas/queue.h>
#include <villas/sample.hpp>
#include <villas/stats.hpp>

#if defined(LIBNL3_ROUTE_FOUND) && defined(__linux__)
#define WITH_NETEM
#endif // LIBNL3_ROUTE_FOUND

// Forward declarations
#ifdef WITH_NETEM
struct rtnl_qdisc;
struct rtnl_cls;
#endif // WITH_NETEM

#define RE_NODE_NAME "[a-z0-9_-]{2,32}"

namespace villas {
namespace node {

// Forward declarations
class NodeFactory;
class SuperNode;

/* The class for a node.
 *
 * Every entity which exchanges messages is represented by a node.
 * Nodes can be remote machines and simulators or locally running processes.
 */
class Node {

  friend NodeFactory;

public:
  Logger logger;

  uint64_t sequence_init;
  uint64_t
      sequence; // This is a counter of received samples, in case the node-type does not generate sequence numbers itself.

  NodeDirection in, out;

  PathSourceList sources; // A list of path sources which reference this node.
  PathDestinationList
      destinations; // A list of path destinations which reference this node.

#ifdef __linux__
  int fwmark; // Socket mark for netem, routing and filtering

#ifdef WITH_NETEM
  struct rtnl_qdisc *tc_qdisc;    // libnl3: Network emulator queuing discipline
  struct rtnl_cls *tc_classifier; // libnl3: Firewall mark classifier
#endif                            // WITH_NETEM
#endif                            // __linux__

protected:
  enum State state;

  uuid_t uuid;

  bool enabled;

  Stats::Ptr
      stats; // Statistic counters. This is a pointer to the statistic hooks private data.

  json_t *config; // A JSON object containing the configuration of the node.

  std::string
      name_short; // A short identifier of the node, only used for configuration and logging
  std::string name_long; // Singleton: A string used to print to screen.
  std::string name_full; // Singleton: A string used to print to screen.
  std::string details;

  int affinity; // CPU Affinity of this node

  NodeFactory *factory; // The factory which created this instance

  virtual int _read(struct Sample *smps[], unsigned cnt) { return -1; }

  virtual int _write(struct Sample *smps[], unsigned cnt) { return -1; }

  virtual json_t *_readStatus() const { return nullptr; }

public:
  // Initialize node with default values
  Node(const uuid_t &id = {}, const std::string &name = "");

  // Destroy node by freeing dynamically allocated memory.
  virtual ~Node();

  // Do initialization after parsing the configuration
  virtual int prepare();

  /* Parse settings of a node.
	 *
	 * @param json A JSON object containing the configuration of the node.
	 * @retval 0 Success. Everything went well.
	 * @retval <0 Error. Something went wrong.
	 */
  virtual int parse(json_t *json);

  // Validate node configuration.
  virtual int check();

  // Start operation of a node.
  virtual int start();

  // Stops operation of a node.
  virtual int stop();

  // Pauses operation of a node.
  virtual int pause() {
    if (state != State::STARTED)
      return -1;

    logger->info("Pausing node");

    return 0;
  }

  // Resumes operation of a node.
  virtual int resume() { return 0; }

  // Restarts operation of a node.
  virtual int restart();

  /* Receive multiple messages at once.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * Messages are received with a single recvmsg() syscall by
	 * using gathering techniques (struct iovec).
	 * The messages will be stored in a circular buffer / array @p m.
	 * Indexes used to address @p m will wrap around after len messages.
	 * Some node-types might only support to receive one message at a time.
	 *
	 * @param smps		An array of pointers to memory blocks where the function should store received samples.
	 * @param cnt		The number of samples that are allocated by the calling function.
	 * @return		    The number of messages actually received.
	 */
  int read(struct Sample *smps[], unsigned cnt);

  /* Send multiple messages in a single datagram / packet.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * Messages are sent with a single sendmsg() syscall by
	 * using gathering techniques (struct iovec).
	 * The messages have to be stored in a circular buffer / array m.
	 * So the indexes will wrap around after len.
	 *
	 * @param smps		An array of pointers to memory blocks where samples read from.
	 * @param cnt		The number of samples that are allocated by the calling function.
	 * @return		    The number of messages actually sent.
	 */
  int write(struct Sample *smps[], unsigned cnt);

  // Reverse local and remote socket address.
  virtual int reverse() { return -1; }

  /* Get a list of file descriptors on which the path should poll
	 *  to detect the availability of new samples which can be read.
	 */
  virtual std::vector<int> getPollFDs() { return {}; }

  /* Get a list of socket file descriptors which are used by the node
	 * To perform network IO. We use those to selectively apply network emulation
	 */
  virtual std::vector<int> getNetemFDs() { return {}; }

  /* Get the memory type which this node-type expects.
	 *
	 * This is useful for special node-types like Infiniband, GPUs & FPGAs
	 * which require DMA-backed memory.
	 */
  virtual struct villas::node::memory::Type *getMemoryType() {
    return villas::node::memory::default_type;
  }

  // Get the factory which was used to construct this node.
  villas::node::NodeFactory *getFactory() const { return factory; }

  /* Return a pointer to a string which should be used to print this node.
	 *
	 * @param n A pointer to the node structure.
	 */
  std::string getNameShort() const { return name_short; }

  // Return a pointer to a string which should be used to print this node.
  const std::string &getName() const { return name_long; }

  // Get the full name including type and details of the node.
  const std::string &getNameFull();

  // Just get the config details of this node as a string
  virtual const std::string &getDetails() {
    static std::string empty;

    return empty;
  }

  /* Return a pointer to a string which should be used to print this node.
	 *
	 * @param n A pointer to the node structure.
	 */
  const std::string &getNameLong();

  /* Return a list of signals which are sent to this node.
	 *
	 * This list is derived from the path which uses the node as destination.
	 */
  SignalList::Ptr getOutputSignals(bool after_hooks = true) const;
  SignalList::Ptr getInputSignals(bool after_hooks = true) const;

  // Get the number of input signals (received by this node)
  unsigned getInputSignalsMaxCount() const;

  // Get the number of output signals (send out via this node)
  unsigned getOutputSignalsMaxCount() const;

  void swapSignals();

  // Get the node configuration as JSON.
  json_t *getConfig() { return config; }

  // Get the state of this node.
  enum State getState() const { return state; }

  // Set the state of this node.
  void setState(enum State s) { state = s; }

  // Get the UUID of this node.
  const uuid_t &getUuid() const { return uuid; }

  std::shared_ptr<Stats> getStats() { return stats; }

  void setStats(std::shared_ptr<Stats> sts) { stats = sts; }

  void setEnabled(bool en) { enabled = en; }

  friend std::ostream &operator<<(std::ostream &os, const Node &n) {
    os << n.getName();

    return os;
  }

  virtual json_t *toJson() const;

  static bool isValidName(const std::string &name);

  bool isEnabled() const { return enabled; }
};

class NodeFactory : public villas::plugin::Plugin {

  friend Node;

protected:
  virtual void init(Node *n) {
    n->logger = getLogger();
    n->factory = this;

    n->name_long = fmt::format(CLR_RED("{}") "(" CLR_YEL("{}") ")",
                               n->name_short, getName());

    instances.push_back(n);
  }

  State state;

public:
  enum class Flags {
    SUPPORTS_POLL = (1 << 0),
    SUPPORTS_READ = (1 << 1),
    SUPPORTS_WRITE = (1 << 2),
    REQUIRES_WEB = (1 << 3),
    PROVIDES_SIGNALS = (1 << 4),
    INTERNAL = (1 << 5),
    HIDDEN = (1 << 6)
  };

  NodeList instances;

  NodeFactory() : Plugin() { state = State::INITIALIZED; }

  virtual Node *make(const uuid_t &id = {}, const std::string &nme = "") = 0;

  static Node *make(json_t *json, const uuid_t &id,
                    const std::string &name = "");

  static Node *make(const std::string &type, const uuid_t &id = {},
                    const std::string &name = "");

  virtual std::string getType() const { return "node"; }

  friend std::ostream &operator<<(std::ostream &os, const NodeFactory &f) {
    os << f.getName();

    return os;
  }

  virtual int getFlags() const { return 0; }

  virtual int getVectorize() const { return 0; }

  bool isInternal() const { return getFlags() & (int)Flags::INTERNAL; }

  bool isHidden() const {
    return isInternal() || getFlags() & (int)Flags::HIDDEN;
  }

  virtual int start(SuperNode *sn);

  virtual int stop();

  State getState() const { return state; }
};

template <typename T, const char *name, const char *desc, int flags = 0,
          int vectorize = 0>
class NodePlugin : public NodeFactory {

public:
  virtual Node *make(const uuid_t &id = {}, const std::string &nme = "") {
    T *n = new T(id, nme);

    init(n);

    return n;
  }

  virtual int getFlags() const { return flags; }

  virtual int getVectorize() const { return vectorize; }

  virtual std::string getName() const { return name; }

  virtual std::string getDescription() const { return desc; }
};

} // namespace node
} // namespace villas

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<villas::node::Node> : public fmt::ostream_formatter {};
template <>
class fmt::formatter<villas::node::NodeFactory>
    : public fmt::ostream_formatter {};
#endif
