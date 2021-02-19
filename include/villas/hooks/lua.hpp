/** Lua expressions hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#pragma once

#include <villas/hook.hpp>

#include <vector>

extern "C" {
	#include "lua.h"
};

namespace villas {
namespace node {

/* Forward declarations */
class LuaHook;

class LuaSignalExpression {

protected:
	int cookie;
	int index;

	LuaHook *hook;

	std::string expression;

	struct signal *signal;

	json_t *cfg;

public:
	LuaSignalExpression(int index, json_t *json_sig, LuaHook *h);

	~LuaSignalExpression();

	void prepare();

	void loadExpression(const std::string &expr);

	void evaluate(union signal_data *data);
};

class LuaHook : public Hook {

	friend LuaSignalExpression;

protected:
	std::string script;
	std::vector<LuaSignalExpression> expressions;

	lua_State *L;

	bool useNames;
	bool hasExpressions;

	/* Function indizes */
	struct {
		int start;
		int stop;
		int restart;
		int process;
		int periodic;
		int prepare;
	} functions;

	void parseExpressions(json_t *json_sigs);

public:
	LuaHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true);

	virtual ~LuaHook();

	virtual void parse(json_t *c);

	virtual void prepare();

	/** Called whenever a hook is started; before threads are created. */
	virtual void start();

	/** Called whenever a hook is stopped; after threads are destoyed. */
	virtual void stop();

	/** Called whenever a new simulation case is started. This is detected by a sequence no equal to zero. */
	virtual void restart();

	/** Called whenever a sample is processed. */
	virtual Reason process(sample *smp);
};

} /* namespace node */
} /* namespace villas */

/** @} */
