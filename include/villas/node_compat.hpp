/* Node compatability layer for C++.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <jansson.h>

#include <villas/node.hpp>
#include <villas/node_compat_type.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

// Forward declarations
class NodeCompatFactory;

class NodeCompat : public Node {

protected:
  NodeCompatType *_vt; // Virtual functions (C++ OOP style)
  void *_vd;           // Virtual data (used by struct vnode::_vt functions)

  std::string _details;

  int _read(struct Sample *smps[], unsigned cnt) override;
  int _write(struct Sample *smps[], unsigned cnt) override;

public:
  NodeCompat *node;
  json_t *cfg;

  NodeCompat(NodeCompatType *vt, const uuid_t &id, const std::string &name);
  NodeCompat(const NodeCompat &n);

  NodeCompat &operator=(const NodeCompat &other);

  virtual ~NodeCompat();

  template <typename T> T *getData() { return static_cast<T *>(_vd); }

  NodeCompatType *getType() const { return _vt; }

  /* Parse node connection details.

   *
   * @param json	A JSON object containing the configuration of the node.
   * @retval 0 	Success. Everything went well.
   * @retval <0	Error. Something went wrong.
   */
  int parse(json_t *json) override;

  // Returns a string with a textual represenation of this node.
  const std::string &getDetails() override;

  /* Check the current node configuration for plausability and errors.

   *
   * @retval 0 	Success. Node configuration is good.
   * @retval <0	Error. The node configuration is bogus.
   */
  int check() override;

  int prepare() override;

  /* Start this node.

   *
   * @retval 0	Success. Everything went well.
   * @retval <0	Error. Something went wrong.
   */
  int start() override;

  /* Stop this node.

   *
   * @retval 0	Success. Everything went well.
   * @retval <0	Error. Something went wrong.
   */
  int stop() override;

  /* Restart this node.

   *
   * @param n	A pointer to the node object.
   * @retval 0	Success. Everything went well.
   * @retval <0	Error. Something went wrong.
   */
  int restart() override;

  /* Pause this node.

   *
   * @param n	A pointer to the node object.
   * @retval 0	Success. Everything went well.
   * @retval <0	Error. Something went wrong.
   */
  int pause() override;

  /* Resume this node.

   *
   * @retval 0	Success. Everything went well.
   * @retval <0	Error. Something went wrong.
   */
  int resume() override;

  /* Reverse source and destination of a node.

   */
  int reverse() override;

  std::vector<int> getPollFDs() override;

  // Get list of socket file descriptors for configuring network emulation.
  std::vector<int> getNetemFDs() override;

  // Return a memory allocator which should be used for sample pools passed to this node.
  struct memory::Type *getMemoryType() override;
};

class NodeCompatFactory : public NodeFactory {

protected:
  NodeCompatType *_vt;

public:
  NodeCompatFactory(NodeCompatType *vt) : NodeFactory(), _vt(vt) {}

  Node *make(const uuid_t &id = {}, const std::string &name = "") override;

  /// Get plugin name
  std::string getName() const override { return _vt->name; }

  /// Get plugin description
  std::string getDescription() const override { return _vt->description; }

  int getFlags() const override;

  int getVectorize() const override { return _vt->vectorize; }

  int start(SuperNode *sn) override;

  int stop() override;
};

} // namespace node
} // namespace villas
