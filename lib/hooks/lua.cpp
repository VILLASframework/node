
/** Lua expressions hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

/** @addtogroup hooks Hook functions
 * @{
 */

#include <vector>
#include <map>

extern "C" {
	#include "lua.h"
	#include "lualib.h"
	#include "lauxlib.h"
};

#include <villas/exceptions.hpp>
#include <villas/hooks/lua.hpp>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/sample.h>
#include <villas/signal.h>
#include <villas/signal_list.h>
#include <villas/utils.hpp>

using namespace villas;

class LuaError : public RuntimeError {

protected:
	lua_State *L;
	int err;

public:
	LuaError(lua_State *l, int e) :
		RuntimeError(""),
		L(l),
		err(e)
	{ }

	virtual const char * what() const noexcept
	{
		switch (err) {
			case LUA_ERRSYNTAX:
				return "Lua: Syntax error";

			case LUA_ERRMEM:
				return "Lua: memory allocation error";

			case LUA_ERRFILE:
				return "Lua: failed to open Lua script";

			case LUA_ERRRUN:
				return lua_tostring(L, -1);

			case LUA_ERRERR:
				return "Lua: failed to call error handler";

			default:
				return "Lua: unknown error";
		}
	}
};

static void
lua_pushtimespec(lua_State *L, struct timespec *ts)
{
	lua_createtable(L, 2, 0);

	lua_pushnumber(L, ts->tv_sec);
	lua_rawseti(L, -2, 0);

	lua_pushnumber(L, ts->tv_nsec);
	lua_rawseti(L, -2, 1);
}

static void
lua_totimespec(lua_State *L, struct timespec *ts)
{
	lua_rawgeti(L, -1, 0);
	ts->tv_sec = lua_tonumber(L, -1);

	lua_rawgeti(L, -2, 1);
	ts->tv_sec = lua_tonumber(L, -1);

	lua_pop(L, 2);
}

static void
lua_tosignaldata(lua_State *L, union signal_data *data, enum SignalType targetType, int idx = -1)
{
	int ret;
	enum SignalType type;

	ret = lua_type(L, idx);
	switch (ret) {
		case LUA_TBOOLEAN:
			data->b = lua_toboolean(L, idx);
			type = SignalType::BOOLEAN;
			break;

		case LUA_TNUMBER:
			data->f = lua_tonumber(L, idx);
			type = SignalType::FLOAT;
			break;

		default:
			return;
	}

	signal_data_cast(data, type, targetType);

	return;
}

static void
lua_tosample(lua_State *L, struct sample *smp, struct vlist *signals, bool use_names = true, int idx = -1)
{
	lua_getfield(L, idx, "sequence");
	smp->sequence = lua_tonumber(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, idx, "flags");
	smp->flags = lua_tonumber(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, idx, "ts_origin");
	lua_totimespec(L, &smp->ts.origin);
	lua_pop(L, 1);

	lua_getfield(L, idx, "ts_received");
	lua_totimespec(L, &smp->ts.received);
	lua_pop(L, 1);

	lua_getfield(L, idx, "data");

	smp->length = lua_objlen(L, -1);

	if (smp->length > vlist_length(signals))
		smp->length = vlist_length(signals);

	if (smp->length > smp->capacity)
		smp->length = smp->capacity;

	for (unsigned i = 0; i < smp->length; i++) {
		struct signal *sig = (struct signal *) vlist_at(signals, i);

		if (use_names)
			lua_getfield(L, -1, sig->name);
		else
			lua_rawgeti(L, -1, i);

		lua_tosignaldata(L, &smp->data[i], sig->type);

		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}

static void
lua_pushsample(lua_State *L, struct sample *smp, bool use_names = true)
{
	lua_createtable(L, 0, 5);

	lua_pushnumber(L, smp->sequence);
	lua_setfield(L, -2, "sequence");

	lua_pushnumber(L, smp->flags);
	lua_setfield(L, -2, "flags");

	lua_pushtimespec(L, &smp->ts.origin);
	lua_setfield(L, -2, "ts_origin");

	lua_pushtimespec(L, &smp->ts.received);
	lua_setfield(L, -2, "ts_received");

	lua_createtable(L, smp->length, 0);

	for (unsigned i = 0; i < smp->length; i++) {
		struct signal *sig = (struct signal *) vlist_at(smp->signals, i);
		union signal_data *data = &smp->data[i];

		switch (sig->type) {
			case SignalType::FLOAT:
				lua_pushnumber(L, data->f);
				break;

			case SignalType::INTEGER:
				lua_pushinteger(L, data->i);
				break;

			case SignalType::BOOLEAN:
				lua_pushboolean(L, data->b);
				break;

			case SignalType::COMPLEX:
			case SignalType::INVALID:
			default:
				continue; /* we skip unknown types. Lua will see a nil value in the table */
		}

		if (use_names)
			lua_setfield(L, -1, sig->name);
		else
			lua_rawseti(L, -2, i);
	}

	lua_setfield(L, -2, "data");
}

namespace villas {
namespace node {


LuaSignalExpression::LuaSignalExpression(int index, json_t *json_sig, LuaHook *h) :
	cookie(0),
	index(-1),
	hook(h)
{
	int ret;

	const char *expr;

	json_error_t err;
	ret = json_unpack_ex(json_sig, &err, 0, "{ s: s }",
		"script", &expr
	);
	if (ret)
		throw ConfigError(json_sig, err, "node-config-hook-lua-signals");

	cfg = json_sig;

	expression = expr;
}

LuaSignalExpression::~LuaSignalExpression()
{
	if (cookie)
		luaL_unref(hook->L, LUA_REGISTRYINDEX, cookie);
}

void
LuaSignalExpression::prepare()
{
	signal = (struct signal *) vlist_at(&hook->signals, index);

	loadExpression(expression);
}

void
LuaSignalExpression::loadExpression(const std::string &expr)
{
	/* Release previous expression */
	if (cookie)
		luaL_unref(hook->L, LUA_REGISTRYINDEX, cookie);

	auto fexpr = fmt::format("return {}", expr);

	int err = luaL_loadstring(hook->L, fexpr.c_str());
	if (err)
		throw ConfigError(cfg, "node-config-hook-lua-signals", "Failed to load Lua expression: {}", lua_tostring(hook->L, -1));

	cookie = luaL_ref(hook->L, LUA_REGISTRYINDEX);
}

void
LuaSignalExpression::evaluate(union signal_data *data)
{
	int err;

	lua_rawgeti(hook->L, LUA_REGISTRYINDEX, cookie);

	err = lua_pcall(hook->L, 0, 1, 0);
	if (err) {
		throw RuntimeError("Lua: Evaluation failed: {}", lua_tostring(hook->L, -1));
		lua_pop(hook->L, 1);
	}

	lua_tosignaldata(hook->L, data, signal->type);

	lua_pop(hook->L, 1);
}

void
LuaHook::parseExpressions(json_t *json_sigs)
{
	size_t i;
	json_t *json_sig;

	if (!json_is_array(json_sigs))
		throw ConfigError(json_sigs, "node-config-hook-lua-signals", "Setting 'signals' must be a list of dicts");

	json_array_foreach(json_sigs, i, json_sig)
		expressions.emplace_back(i, json_sig, this);
}

LuaHook::LuaHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en) :
	Hook(p, n, fl, prio, en),
	L(luaL_newstate()),
	useNames(true),
	hasExpressions(false)
{ }

LuaHook::~LuaHook()
{
	lua_close(L);
}

void
LuaHook::parse(json_t *c)
{
	int ret;
	const char *script_str = nullptr;
	int names = 1;
	json_error_t err;
	json_t *json_signals = nullptr;

	assert(state != State::STARTED);

	Hook::parse(c);

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: o, s?: b }",
		"script", &script_str,
		"signals", &json_signals,
		"use_names", &names
	);
	if (ret)
		throw ConfigError(cfg, err, "node-config-hook-lua");

	useNames = names;

	if (script_str)
		script = script_str;

	if (json_signals)
		parseExpressions(json_signals);

	state = State::PARSED;
}

void
LuaHook::prepare()
{
	int ret;

	/* Load Lua standard libraries */
	luaL_openlibs(L);

	/* Load our Lua script */
	logger->debug("Loading Lua script: %s", script.c_str());

	ret = luaL_loadfile(L, script.c_str());
	if (ret)
		throw LuaError(L, ret);

	ret = lua_pcall(L, 0, LUA_MULTRET, 0);
	if (ret)
		throw LuaError(L, ret);

	/* Prepare Lua expressions */
	if (hasExpressions) {
		// signal_list_clear(&signals);

		for (auto &expr : expressions) {
			expr.prepare();
		}
	}

	/* Lookup functions */
	std::map<const char *, int *> funcs = {
		{ "start",    &functions.start    },
		{ "stop",     &functions.stop     },
		{ "restart",  &functions.restart  },
		{ "prepare",  &functions.prepare  },
		{ "periodic", &functions.periodic },
		{ "process",  &functions.process  }
	};

	for (auto it : funcs) {
		lua_getglobal(L, it.first);

		ret = lua_type(L, -1);
		if (ret == LUA_TFUNCTION) {
			logger->debug("Found Lua function: %s()", it.first);
			*(it.second) = lua_gettop(L);
		}
		else {
			*(it.second) = 0;
			lua_pop(L, 1);
		}
	}

	if (functions.process && hasExpressions)
		throw ConfigError(cfg, "node-config-hook-lua", "Lua expressions in the signal definition can not be combined with a process() function in the script.");

	if (functions.prepare) {
		logger->debug("Executing Lua function: prepare()");
		lua_pushvalue(L, functions.prepare);
		lua_call(L, 0, 0);
	}
}

/** Called whenever a hook is started; before threads are created. */
void
LuaHook::start()
{
	assert(state == State::PREPARED);

	if (functions.start) {
		logger->debug("Executing Lua function: start()");
		lua_pushvalue(L, functions.start);
		lua_call(L, 0, 0);
	}

	state = State::STARTED;
}

/** Called whenever a hook is stopped; after threads are destoyed. */
void
LuaHook::stop()
{
	assert(state == State::STARTED);

	if (functions.stop) {
		logger->debug("Executing Lua function: stop()");
		lua_pushvalue(L, functions.stop);
		lua_call(L, 0, 0);
	}

	state = State::STOPPED;
}

/** Called whenever a new simulation case is started. This is detected by a sequence no equal to zero. */
void
LuaHook::restart()
{
	assert(state == State::STARTED);

	if (functions.restart) {
		logger->debug("Executing Lua function: restart()");
		lua_pushvalue(L, functions.restart);
		lua_call(L, 0, 0);
	}
	else
		Hook::restart();
}

/** Called whenever a sample is processed. */
Hook::Reason
LuaHook::process(sample *smp)
{
	/* First evaluate expressions */
	if (hasExpressions) {
		for (unsigned i = 0; i < expressions.size(); i++)
			expressions[i].evaluate(&smp->data[i]);

		smp->length = expressions.size();

		smp->signals = &signals;
	}

	/* Run the process() function of the script */
	if (functions.process) {
		logger->debug("Executing Lua function: process(smp)");

		lua_pushvalue(L, functions.process);

		lua_pushsample(L, smp, useNames);
		lua_call(L, 1, 1);
		lua_tosample(L, smp, &signals, useNames);
	}

	return Reason::OK;
}

/* Register hook */
static char n[] = "lua";
static char d[] = "Implement hook in Lua";
static HookPlugin<LuaHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH, 1> p;

} /* namespace node */
} /* namespace villas */

/** @} */
