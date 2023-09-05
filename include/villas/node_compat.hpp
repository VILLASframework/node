/* Node compatability layer for C++.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <jansson.h>

#include <villas/sample.hpp>
#include <villas/node.hpp>
#include <villas/node_compat_type.hpp>

namespace villas {
namespace node {

// Forward declarations
class NodeCompatFactory;

class NodeCompat : public Node {

protected:
	struct NodeCompatType *_vt;	// Virtual functions (C++ OOP style)
	void *_vd;			// Virtual data (used by struct vnode::_vt functions)

	std::string _details;

	virtual
	int _read(struct Sample *smps[], unsigned cnt);

	virtual
	int _write(struct Sample *smps[], unsigned cnt);

public:
	NodeCompat *node;
	json_t *cfg;

	NodeCompat(struct NodeCompatType *vt, const uuid_t &id, const std::string &name);
	NodeCompat(const NodeCompat& n);

	NodeCompat& operator=(const NodeCompat& other);

	virtual
	~NodeCompat();

	template<typename T>
	T * getData()
	{
		return static_cast<T *>(_vd);
	}

	virtual
	NodeCompatType * getType() const
	{
		return _vt;
	}

	/* Parse node connection details.

	 *
	 * @param json	A JSON object containing the configuration of the node.
	 * @retval 0 	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	virtual
	int parse(json_t *json);

	// Returns a string with a textual represenation of this node.
	virtual
	const std::string & getDetails();

	/* Check the current node configuration for plausability and errors.

	 *
	 * @retval 0 	Success. Node configuration is good.
	 * @retval <0	Error. The node configuration is bogus.
	 */
	virtual
	int check();

	virtual
	int prepare();

	/* Start this node.

	 *
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	virtual
	int start();

	/* Stop this node.

	 *
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	virtual
	int stop();

	/* Restart this node.

	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	virtual
	int restart();

	/* Pause this node.

	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	virtual
	int pause();

	/* Resume this node.

	 *
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	virtual
	int resume();

	/* Reverse source and destination of a node.

	 */
	virtual
	int reverse();

	virtual
	std::vector<int> getPollFDs();

	// Get list of socket file descriptors for configuring network emulation.
	virtual
	std::vector<int> getNetemFDs();

	// Return a memory allocator which should be used for sample pools passed to this node.
	virtual
	struct memory::Type * getMemoryType();
};

class NodeCompatFactory : public NodeFactory {

protected:
	struct NodeCompatType *_vt;

public:
	NodeCompatFactory(struct NodeCompatType *vt) :
		NodeFactory(),
		_vt(vt)
	{ }

	virtual
	Node * make(const uuid_t &id = {}, const std::string &name = "");

	/// Get plugin name
	virtual
	std::string getName() const
	{
		return _vt->name;
	}

	/// Get plugin description
	virtual
	std::string getDescription() const
	{
		return _vt->description;
	}

	virtual
	int getFlags() const;

	virtual
	int getVectorize() const
	{
		return _vt->vectorize;
	}

	virtual
	int start(SuperNode *sn);

	virtual
	int stop();
};

} // namespace node
} // namespace villas
