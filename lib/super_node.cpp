/* The super node object holding the state of the application.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <villas/config_helper.hpp>
#include <villas/hook_list.hpp>
#include <villas/kernel/if.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/log.hpp>
#include <villas/node.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/node/memory.hpp>
#include <villas/path.hpp>
#include <villas/super_node.hpp>
#include <villas/timing.hpp>
#include <villas/uuid.hpp>

#ifdef WITH_NETEM
#include <villas/kernel/nl.hpp>
#endif

using namespace villas;
using namespace villas::node;

SuperNode::SuperNode()
    : state(State::INITIALIZED), idleStop(-1),
#ifdef WITH_API
      api(this),
#endif
#ifdef WITH_WEB
#ifdef WITH_API
      web(&api),
#else
      web(),
#endif
#endif
      priority(0), affinity(0), hugepages(DEFAULT_NR_HUGEPAGES), statsRate(1.0),
      task(CLOCK_REALTIME), started(time_now()) {
  int ret;

  char hname[128];
  ret = gethostname(hname, sizeof(hname));
  if (ret)
    throw SystemError("Failed to determine hostname");

  // Default UUID is derived from hostname
  uuid::generateFromString(uuid, hname);

#ifdef WITH_NETEM
  kernel::nl::init(); // Fill link cache
#endif                // WITH_NETEM

  logger = logging.get("super_node");
}

void SuperNode::parse(const std::string &u) {
  config.root = config.load(u);

  parse(config.root);
}

void SuperNode::parse(json_t *root) {
  int ret;

  assert(state == State::PARSED || state == State::INITIALIZED ||
         state == State::CHECKED);

  const char *uuid_str = nullptr;

  json_t *json_nodes = nullptr;
  json_t *json_paths = nullptr;
  json_t *json_logging = nullptr;
  json_t *json_http = nullptr;

  json_error_t err;

  idleStop = 1;

  ret =
      json_unpack_ex(root, &err, 0,
                     "{ s?: F, s?: o, s?: o, s?: o, s?: o, s?: i, s?: i, s?: "
                     "i, s?: b, s?: s }",
                     "stats", &statsRate, "http", &json_http, "logging",
                     &json_logging, "nodes", &json_nodes, "paths", &json_paths,
                     "hugepages", &hugepages, "affinity", &affinity, "priority",
                     &priority, "idle_stop", &idleStop, "uuid", &uuid_str);
  if (ret)
    throw ConfigError(root, err, "node-config",
                      "Unpacking top-level config failed");

  if (uuid_str) {
    ret = uuid_parse(uuid_str, uuid);
    if (ret)
      throw ConfigError(root, "node-config-uuid", "Failed to parse UUID: {}",
                        uuid_str);
  }

#ifdef WITH_WEB
  if (json_http)
    web.parse(json_http);
#endif // WITH_WEB

  if (json_logging)
    logging.parse(json_logging);

  // Parse nodes
  if (json_nodes) {
    if (!json_is_object(json_nodes))
      throw ConfigError(
          json_nodes, "node-config-nodes",
          "Setting 'nodes' must be a group with node name => group mappings.");

    const char *node_name;
    json_t *json_node;
    json_object_foreach(json_nodes, node_name, json_node) {
      uuid_t node_uuid;
      const char *node_type;
      const char *node_uuid_str = nullptr;

      ret = Node::isValidName(node_name);
      if (!ret)
        throw RuntimeError("Invalid name for node: {}", node_name);

      ret = json_unpack_ex(json_node, &err, 0, "{ s: s, s?: s }", "type",
                           &node_type, "uuid", &node_uuid_str);
      if (ret)
        throw ConfigError(root, err, "node-config-node-type",
                          "Failed to parse type of node '{}'", node_name);

      if (node_uuid_str) {
        ret = uuid_parse(uuid_str, uuid);
        if (ret)
          throw ConfigError(json_node, "node-config-node-uuid",
                            "Failed to parse UUID: {}", uuid_str);
      } else
        // Generate UUID from node name and super-node UUID
        uuid::generateFromString(node_uuid, node_name, uuid::toString(uuid));

      auto *n = NodeFactory::make(node_type, node_uuid, node_name);
      if (!n)
        throw MemoryAllocationError();

      ret = n->parse(json_node);
      if (ret) {
        auto config_id = fmt::format("node-config-node-{}", node_type);
        throw ConfigError(json_node, config_id,
                          "Failed to parse configuration of node '{}'",
                          node_name);
      }

      nodes.push_back(n);
    }
  }

  // Parse paths
  if (json_paths) {
    if (!json_is_array(json_paths))
      logger->warn("Setting 'paths' must be a list of objects");

    size_t i;
    json_t *json_path;
    json_array_foreach(json_paths, i, json_path) {
    parse:
      auto *p = new Path();
      if (!p)
        throw MemoryAllocationError();

      p->parse(json_path, nodes, uuid);

      paths.push_back(p);

      if (p->isReversed()) {
        // Only simple paths can be reversed
        ret = p->isSimple();
        if (!ret)
          throw RuntimeError("Complex paths can not be reversed!");

        // Parse a second time with in/out reversed
        json_path = json_copy(json_path);

        json_t *json_in = json_object_get(json_path, "in");
        json_t *json_out = json_object_get(json_path, "out");

        if (json_equal(json_in, json_out))
          throw RuntimeError(
              "Can not reverse path with identical in/out nodes!");

        json_object_set(json_path, "reverse", json_false());
        json_object_set(json_path, "in", json_out);
        json_object_set(json_path, "out", json_in);

        goto parse;
      }
    }
  }

  state = State::PARSED;
}

void SuperNode::check() {
  int ret;

  assert(state == State::INITIALIZED || state == State::PARSED ||
         state == State::CHECKED);

  for (auto *n : nodes) {
    ret = n->check();
    if (ret)
      throw RuntimeError("Invalid configuration for node {}", n->getName());
  }

  for (auto *p : paths)
    p->check();

  state = State::CHECKED;
}

void SuperNode::prepareNodeTypes() {
  int ret;

  for (auto *n : nodes) {
    auto *nf = n->getFactory();

    if (nf->getState() == State::STARTED)
      continue;

    ret = nf->start(this);
    if (ret)
      throw RuntimeError("Failed to start node-type: {}",
                         n->getFactory()->getName());
  }
}

void SuperNode::startInterfaces() {
#ifdef WITH_NETEM
  int ret;

  for (auto *i : interfaces) {
    ret = i->start();
    if (ret)
      throw RuntimeError("Failed to start network interface: {}", i->getName());
  }
#endif // WITH_NETEM
}

void SuperNode::startNodes() {
  int ret;

  for (auto *n : nodes) {
    if (!n->isEnabled())
      continue;

    ret = n->start();
    if (ret)
      throw RuntimeError("Failed to start node: {}", n->getName());
  }
}

void SuperNode::startPaths() {
  for (auto *p : paths) {
    if (!p->isEnabled())
      continue;

    p->start();
  }
}

void SuperNode::prepareNodes() {
  int ret;

  for (auto *n : nodes) {
    if (!n->isEnabled())
      continue;

    ret = n->prepare();
    if (ret)
      throw RuntimeError("Failed to prepare node: {}", n->getName());
  }
}

void SuperNode::preparePaths() {
  for (auto *p : paths) {
    if (!p->isEnabled())
      continue;

    p->prepare(nodes);
  }
}

void SuperNode::prepare() {
  int ret;

  assert(state == State::CHECKED);

  ret = memory::init(hugepages);
  if (ret)
    throw RuntimeError("Failed to initialize memory system");

  kernel::rt::init(priority, affinity);

  prepareNodeTypes();
  prepareNodes();
  preparePaths();

  for (auto *n : nodes) {
    if (n->sources.size() == 0 && n->destinations.size() == 0) {
      logger->info("Node {} is not used by any path. Disabling...",
                   n->getName());
      n->setEnabled(false);
    }
  }

  state = State::PREPARED;
}

void SuperNode::start() {
  assert(state == State::PREPARED);

#ifdef WITH_API
  api.start();
#endif

#ifdef WITH_WEB
  web.start();
#endif

  startInterfaces();
  startNodes();
  startPaths();

  if (statsRate > 0) // A rate <0 will disable the periodic stats
    task.setRate(statsRate);

  Stats::printHeader(Stats::Format::HUMAN);

  state = State::STARTED;
}

void SuperNode::stopPaths() {
  for (auto *p : paths) {
    if (p->getState() == State::STARTED || p->getState() == State::PAUSED)
      p->stop();
  }
}

void SuperNode::stopNodes() {
  int ret;

  for (auto *n : nodes) {
    if (n->getState() == State::STARTED || n->getState() == State::PAUSED ||
        n->getState() == State::STOPPING) {
      ret = n->stop();
      if (ret)
        throw RuntimeError("Failed to stop node: {}", n->getName());
    }
  }
}

void SuperNode::stopNodeTypes() {
  int ret;

  for (auto *n : nodes) {
    auto *nf = n->getFactory();

    if (nf->getState() != State::STARTED)
      continue;

    ret = nf->stop();
    if (ret)
      throw RuntimeError("Failed to stop node-type: {}",
                         n->getFactory()->getName());
  }
}

void SuperNode::stopInterfaces() {
#ifdef WITH_NETEM
  int ret;

  for (auto *i : interfaces) {
    ret = i->stop();
    if (ret)
      throw RuntimeError("Failed to stop interface: {}", i->getName());
  }
#endif // WITH_NETEM
}

void SuperNode::stop() {
  stopNodes();
  stopPaths();
  stopNodeTypes();
  stopInterfaces();

#ifdef WITH_API
  api.stop();
#endif

#ifdef WITH_WEB
  web.stop();
#endif

  state = State::STOPPED;
}

void SuperNode::run() {
  int ret;

  while (state == State::STARTED) {
    task.wait();

    ret = periodic();
    if (ret)
      state = State::STOPPING;
  }
}

SuperNode::~SuperNode() {
  assert(state == State::INITIALIZED || state == State::PARSED ||
         state == State::CHECKED || state == State::STOPPED ||
         state == State::PREPARED);

  for (auto *p : paths)
    delete p;

  for (auto *n : nodes)
    delete n;
}

int SuperNode::periodic() {
  int started = 0;

  for (auto *p : paths) {
    if (p->getState() == State::STARTED) {
      started++;

#ifdef WITH_HOOKS
      p->hooks.periodic();
#endif // WITH_HOOKS
    }
  }

  for (auto *n : nodes) {
    if (n->getState() == State::STARTED) {
#ifdef WITH_HOOKS
      n->in.hooks.periodic();
      n->out.hooks.periodic();
#endif // WITH_HOOKS
    }
  }

  if (idleStop > 0 && state == State::STARTED && started == 0) {
    logger->info("No more active paths. Stopping super-node");

    return -1;
  }

  return 0;
}

#ifdef WITH_GRAPHVIZ
static void set_attr(void *ptr, const std::string &key,
                     const std::string &value, bool html = false) {
  Agraph_t *g = agraphof(ptr);

  char *d = (char *)"";
  char *k = (char *)key.c_str();
  char *v = (char *)value.c_str();
  char *vd = html ? agstrdup_html(g, v) : agstrdup(g, v);

  agsafeset(ptr, k, vd, d);
}

graph_t *SuperNode::getGraph() {
  Agraph_t *g;
  Agnode_t *m;

  g = agopen((char *)"g", Agdirected, 0);

  std::map<Node *, Agnode_t *> nodeMap;

  for (auto *n : nodes) {
    nodeMap[n] = agnode(g, (char *)n->getNameShort().c_str(), 1);

    set_attr(nodeMap[n], "shape", "ellipse");
    set_attr(nodeMap[n], "tooltip",
             fmt::format("type={}, uuid={}", n->getFactory()->getName(),
                         uuid::toString(n->getUuid()).c_str()));
    // set_attr(nodeMap[n], "fixedsize", "true");
    // set_attr(nodeMap[n], "width", "0.15");
    // set_attr(nodeMap[n], "height", "0.15");
  }

  unsigned i = 0;
  for (auto *p : paths) {
    auto name = fmt::format("path_{}", i++);

    m = agnode(g, (char *)name.c_str(), 1);

    set_attr(m, "shape", "box");
    set_attr(m, "tooltip",
             fmt::format("uuid={}", uuid::toString(p->getUuid()).c_str()));

    for (auto ps : p->sources)
      agedge(g, nodeMap[ps->getNode()], m, nullptr, 1);

    for (auto pd : p->destinations)
      agedge(g, m, nodeMap[pd->getNode()], nullptr, 1);
  }

  return g;
}
#endif // WITH_GRAPHVIZ
