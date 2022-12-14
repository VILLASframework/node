/** Lua expressions hook.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <vector>
#include <mutex>

#include <villas/hook.hpp>

extern "C" {
	#include "lua.h"
};

namespace villas {
namespace node {

/* Forward declarations */
class LuaHook;
enum SignalType;

class LuaSignalExpression {

protected:
	int cookie;
	lua_State *L;

	std::string expression;

	json_t *cfg;

public:
	LuaSignalExpression(lua_State *L, json_t *json_sig);

	void prepare();

	void parseExpression(const std::string &expr);

	void evaluate(union SignalData *data, enum SignalType type);
};

class LuaHook : public Hook {

	friend LuaSignalExpression;

private:
	static const int SELF_REFERENCE = 55;

protected:
	std::string script;
	std::vector<LuaSignalExpression> expressions;

	SignalList::Ptr signalsProcessed; /**> Signals as emited by Lua process() function */
	SignalList::Ptr signalsExpressions; /**> Signals as emited by Lua expressions */

	lua_State *L;
	std::mutex mutex;

	bool useNames;
	bool hasExpressions;
	bool needsLocking;

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

	void loadScript();
	void lookupFunctions();
	void setupEnvironment();

	/* Lua functions */

	int luaInfo(lua_State *L);
	int luaWarn(lua_State *L);
	int luaError(lua_State *L);
	int luaDebug(lua_State *L);

	int luaRegisterApiHandler(lua_State *L);

	typedef int (LuaHook::*mem_func)(lua_State * L);

	// This template wraps a member function into a C-style "free" function compatible with lua.
	template <mem_func func>
	static int
	dispatch(lua_State * L) {
		lua_rawgeti(L, LUA_REGISTRYINDEX, SELF_REFERENCE);
		void *vptr = lua_touserdata(L, -1);
		lua_pop(L, 1);

		LuaHook *ptr = static_cast<LuaHook*>(vptr);
		return ((*ptr).*func)(L);
	}

public:
	LuaHook(Path *p, Node *n, int fl, int prio, bool en = true);

	virtual ~LuaHook();

	virtual void parse(json_t *cfg);

	virtual void prepare();

	/** Periodically called by the main thread. */
	virtual void periodic();

	/** Called whenever a hook is started; before threads are created. */
	virtual void start();

	/** Called whenever a hook is stopped; after threads are destoyed. */
	virtual void stop();

	/** Called whenever a new simulation case is started. This is detected by a sequence no equal to zero. */
	virtual void restart();

	/** Called whenever a sample is processed. */
	virtual Reason process(struct Sample *smp);
};


} /* namespace node */
} /* namespace villas */
