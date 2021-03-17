
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
#include <cstdio>

extern "C" {
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
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
		const char *msg;
		switch (err) {
			case LUA_ERRSYNTAX:
				msg = "Syntax error";
				break;

			case LUA_ERRMEM:
				msg = "Memory allocation error";
				break;

			case LUA_ERRFILE:
				msg = "Failed to open Lua script";
				break;

			case LUA_ERRRUN:
				msg = "Runtime error";
				break;

			case LUA_ERRERR:
				msg = "Failed to call error handler";
				break;

			default:
				msg = "Unknown error";
				break;
		}

		char *buf;

		asprintf(&buf, "Lua: %s: %s", msg, lua_tostring(L, -1));

		return buf;
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
	ts->tv_nsec = lua_tonumber(L, -1);

	lua_pop(L, 2);
}

static void
lua_tosignaldata(lua_State *L, union signal_data *data, enum SignalType targetType, int idx = -1)
{
	int luaType;
	enum SignalType type;

	luaType = lua_type(L, idx);
	switch (luaType) {
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
}

static void
lua_tosample(lua_State *L, struct sample *smp, struct vlist *signals, bool use_names = true, int idx = -1)
{
	int ret;

	smp->length = 0;
	smp->flags = 0;

	lua_getfield(L, idx, "sequence");
	ret = lua_type(L, -1);
	if (ret != LUA_TNIL) {
		smp->sequence = lua_tonumber(L, -1);
		smp->flags |= (int) SampleFlags::HAS_SEQUENCE;
	}
	lua_pop(L, 1);

	lua_getfield(L, idx, "ts_origin");
	ret = lua_type(L, -1);
	if (ret != LUA_TNIL) {
		lua_totimespec(L, &smp->ts.origin);
		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;
	}
	lua_pop(L, 1);

	lua_getfield(L, idx, "ts_received");
	ret = lua_type(L, -1);
	if (ret != LUA_TNIL) {
		lua_totimespec(L, &smp->ts.received);
		smp->flags |= (int) SampleFlags::HAS_TS_RECEIVED;
	}
	lua_pop(L, 1);

	lua_getfield(L, idx, "data");
	ret = lua_type(L, -1);
	if (ret != LUA_TNIL) {
		for (unsigned i = 0; i < smp->length; i++) {
			struct signal *sig = (struct signal *) vlist_at(signals, i);

			if (use_names)
				lua_getfield(L, -1, sig->name);
			else
				lua_rawgeti(L, -1, i);

			ret = lua_type(L, -1);
			if (ret != LUA_TNIL)
				lua_tosignaldata(L, &smp->data[i], sig->type, -1);

			lua_pop(L, 1);
		}

		if (smp->length > 0)
			smp->flags |= (int) SampleFlags::HAS_DATA;
	}
	lua_pop(L, 1);
}

static void
lua_pushsample(lua_State *L, struct sample *smp, bool use_names = true)
{
	lua_createtable(L, 0, 5);

	lua_pushnumber(L, smp->flags);
	lua_setfield(L, -2, "flags");

	if (smp->flags & (int) SampleFlags::HAS_SEQUENCE) {
		lua_pushnumber(L, smp->sequence);
		lua_setfield(L, -2, "sequence");
	}

	if (smp->flags & (int) SampleFlags::HAS_TS_ORIGIN) {
		lua_pushtimespec(L, &smp->ts.origin);
		lua_setfield(L, -2, "ts_origin");
	}

	if (smp->flags & (int) SampleFlags::HAS_TS_RECEIVED) {
		lua_pushtimespec(L, &smp->ts.received);
		lua_setfield(L, -2, "ts_received");
	}

	if (smp->flags & (int) SampleFlags::HAS_DATA) {
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
				lua_setfield(L, -2, sig->name);
			else
				lua_rawseti(L, -2, i);
		}

		lua_setfield(L, -2, "data");
	}
}

static void
lua_pushjson(lua_State *L, json_t *json)
{
	size_t i;
	const char *key;
	json_t *json_value;

	switch (json_typeof(json)) {
		case JSON_OBJECT:
			lua_newtable(L);
			json_object_foreach(json, key, json_value) {
				lua_pushjson(L, json_value);
				lua_setfield(L, -2, key);
			}
			break;;

		case JSON_ARRAY:
			lua_newtable(L);
			json_array_foreach(json, i, json_value) {
				lua_pushjson(L, json_value);
				lua_rawseti(L, -2, i);
			}
			break;

		case JSON_STRING:
			lua_pushstring(L, json_string_value(json));
			break;

		case JSON_REAL:
		case JSON_INTEGER:
			lua_pushnumber(L, json_integer_value(json));
			break;

		case JSON_TRUE:
		case JSON_FALSE:
			lua_pushboolean(L, json_boolean_value(json));
			break;

		case JSON_NULL:
			lua_pushnil(L);
			break;
	}
}

static json_t * lua_tojson(lua_State *L, int index = -1)
{
	double n;
	const char *s;
	bool b;

	switch (lua_type(L, index)) {
		case LUA_TFUNCTION:
		case LUA_TUSERDATA:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
		case LUA_TNIL:
			return json_null();

		case LUA_TNUMBER:
			n = lua_tonumber(L, index);
			return n == (int) n
				? json_integer(n)
				: json_real(n);

		case LUA_TBOOLEAN:
			b = lua_toboolean(L, index);
			return json_boolean(b);

		case LUA_TSTRING:
			s = lua_tostring(L, index);
			return json_string(s);

		case LUA_TTABLE: {
			int keys_total = 0, keys_int = 0, key_highest = -1;

			lua_pushnil(L);
			while (lua_next(L, index) != 0) {
				keys_total++;
				if (lua_type(L, -2) == LUA_TNUMBER) {
					int key = lua_tonumber(L, -1);

					if (key == (int) key) {
						keys_int++;
						if (key > key_highest)
						key_highest = key;
					}
				}
				lua_pop(L, 1);
			}

			bool is_array = keys_total == keys_int && key_highest / keys_int > 0.5;

			json_t *json = is_array
				? json_array()
				: json_object();

			lua_pushnil(L);
			while (lua_next(L, index) != 0) {
				json_t *val = lua_tojson(L, -1);
				if (is_array) {
					int key = lua_tonumber(L, -2);
					json_array_set(json, key, val);
				}
				else {
					const char *key = lua_tostring(L, -2);
					if (key) /* Skip table entries whose keys are neither string or number! */
						json_object_set(json, key, val);
				}
				lua_pop(L, 1);
			}

			return json;
		}
	}

	return nullptr;
}

namespace villas {
namespace node {


LuaSignalExpression::LuaSignalExpression(lua_State *l, json_t *json_sig) :
	cookie(0),
	L(l)
{
	int ret;

	json_error_t err;
	const char *expr;

	/* Parse expression */
	ret = json_unpack_ex(json_sig, &err, 0, "{ s: s }",
		"expression", &expr
	);
	if (ret)
		throw ConfigError(json_sig, err, "node-config-hook-lua-signals");

	cfg = json_sig;

	expression = expr;
}

void
LuaSignalExpression::prepare()
{
	parseExpression(expression);
}

void
LuaSignalExpression::parseExpression(const std::string &expr)
{
	/* Release previous expression */
	if (cookie)
		luaL_unref(L, LUA_REGISTRYINDEX, cookie);

	auto fexpr = fmt::format("return {}", expr);

	int err = luaL_loadstring(L, fexpr.c_str());
	if (err)
		throw ConfigError(cfg, "node-config-hook-lua-signals", "Failed to load Lua expression: {}", lua_tostring(L, -1));

	cookie = luaL_ref(L, LUA_REGISTRYINDEX);
}

void
LuaSignalExpression::evaluate(union signal_data *data, enum SignalType type)
{
	int err;

	lua_rawgeti(L, LUA_REGISTRYINDEX, cookie);

	err = lua_pcall(L, 0, 1, 0);
	if (err) {
		throw RuntimeError("Lua: Evaluation failed: {}", lua_tostring(L, -1));
		lua_pop(L, 1);
	}

	lua_tosignaldata(L, data, type, -1);

	lua_pop(L, 1);
}

LuaHook::LuaHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en) :
	Hook(p, n, fl, prio, en),
	L(luaL_newstate()),
	useNames(true),
	hasExpressions(false)
{
	int ret;
	ret = signal_list_init(&signalsProcessed);
	if (ret)
		throw RuntimeError("Failed to initialize signal list");

	ret = signal_list_init(&signalsExpressions);
	if (ret)
		throw RuntimeError("Failed to initialize signal list");
}

LuaHook::~LuaHook()
{
	int ret __attribute__((unused));

	lua_close(L);

	ret = signal_list_destroy(&signalsProcessed);
	ret = signal_list_destroy(&signalsExpressions);
}

void
LuaHook::parseExpressions(json_t *json_sigs)
{
	int ret;
	size_t i;
	json_t *json_sig;

	signal_list_clear(&signalsExpressions);
	ret = signal_list_parse(&signalsExpressions, json_sigs);
	if (ret)
		throw ConfigError(json_sigs, "node-config-hook-lua-signals", "Setting 'signals' must be a list of dicts");

	json_array_foreach(json_sigs, i, json_sig)
		expressions.emplace_back(L, json_sig);

	hasExpressions = true;
}

void
LuaHook::parse(json_t *json)
{
	int ret;
	const char *script_str = nullptr;
	int names = 1;
	json_error_t err;
	json_t *json_signals = nullptr;

	assert(state != State::STARTED);

	Hook::parse(json);

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: o, s?: b }",
		"script", &script_str,
		"signals", &json_signals,
		"use_names", &names
	);
	if (ret)
		throw ConfigError(json, err, "node-config-hook-lua");

	useNames = names;

	if (script_str)
		script = script_str;

	if (json_signals)
		parseExpressions(json_signals);

	state = State::PARSED;
}

void
LuaHook::lookupFunctions()
{
	int ret;

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
			logger->debug("Found Lua function: {}()", it.first);
			*(it.second) = lua_gettop(L);
		}
		else {
			*(it.second) = 0;
			lua_pop(L, 1);
		}
	}
}

void
LuaHook::loadScript()
{
	int ret;

	if (script.empty())
		return; /* No script given. */

	ret = luaL_loadfile(L, script.c_str());
	if (ret)
		throw LuaError(L, ret);

	ret = lua_pcall(L, 0, LUA_MULTRET, 0);
	if (ret)
		throw LuaError(L, ret);
}

int
LuaHook::luaRegisterApiHandler(lua_State *L)
{
	// register_api_handler(path_regex)
	return 0;
}

int
LuaHook::luaInfo(lua_State *L)
{
	logger->info(luaL_checkstring(L, 1));
	return 0;
}

int
LuaHook::luaWarn(lua_State *L)
{
	logger->warn(luaL_checkstring(L, 1));
	return 0;
}

int
LuaHook::luaError(lua_State *L)
{
	logger->error(luaL_checkstring(L, 1));
	return 0;
}

int
LuaHook::luaDebug(lua_State *L)
{
	logger->debug(luaL_checkstring(L, 1));
	return 0;
}

void
LuaHook::setupEnvironment()
{
	lua_pushlightuserdata(L, this);
	lua_rawseti(L, LUA_REGISTRYINDEX, SELF_REFERENCE);

	lua_register(L, "info", &dispatch<&LuaHook::luaInfo>);
	lua_register(L, "warn", &dispatch<&LuaHook::luaWarn>);
	lua_register(L, "error", &dispatch<&LuaHook::luaError>);
	lua_register(L, "debug", &dispatch<&LuaHook::luaDebug>);

	lua_register(L, "register_api_handler", &dispatch<&LuaHook::luaRegisterApiHandler>);
}

void
LuaHook::prepare()
{
	/* Load Lua standard libraries */
	luaL_openlibs(L);

	/* Load our Lua script */
	logger->debug("Loading Lua script: {}", script);

	setupEnvironment();
	loadScript();
	lookupFunctions();

	/* Check if we need to protect the Lua state with a mutex
	 * This is the case if we have a periodic callback defined
	 * As periodic() gets called from the main thread
	 */
	needsLocking = functions.periodic > 0;

	/* Prepare Lua process() */
	if (functions.process) {
		/* We currently do not support the alteration of
		 * signal metadata in process() */
		signal_list_copy(&signalsProcessed, &signals);
	}

	/* Prepare Lua expressions */
	if (hasExpressions) {
		for (auto &expr : expressions)
			expr.prepare();

		signal_list_clear(&signals);
		signal_list_copy(&signals, &signalsExpressions);
	}

	if (!functions.process && !hasExpressions)
		logger->warn("The hook has neither a script or expressions defined. It is a no-op!");

	if (functions.prepare) {
		auto lockScope = needsLocking
			? std::unique_lock<std::mutex>(mutex)
			: std::unique_lock<std::mutex>();

		logger->debug("Executing Lua function: prepare()");
		lua_pushvalue(L, functions.prepare);
		lua_pushjson(L, config);
		int ret = lua_pcall(L, 1, 0, 0);
		if (ret)
			throw LuaError(L, ret);
	}
}

void
LuaHook::start()
{
	assert(state == State::PREPARED);

	auto lockScope = needsLocking
			? std::unique_lock<std::mutex>(mutex)
			: std::unique_lock<std::mutex>();

	if (functions.start) {
		logger->debug("Executing Lua function: start()");
		lua_pushvalue(L, functions.start);
		int ret = lua_pcall(L, 0, 0, 0);
		if (ret)
			throw LuaError(L, ret);
	}

	state = State::STARTED;
}

void
LuaHook::stop()
{
	assert(state == State::STARTED);

	auto lockScope = needsLocking
			? std::unique_lock<std::mutex>(mutex)
			: std::unique_lock<std::mutex>();

	if (functions.stop) {
		logger->debug("Executing Lua function: stop()");
		lua_pushvalue(L, functions.stop);
		int ret = lua_pcall(L, 0, 0, 0);
		if (ret)
			throw LuaError(L, ret);
	}

	state = State::STOPPED;
}

void
LuaHook::restart()
{
	auto lockScope = needsLocking
			? std::unique_lock<std::mutex>(mutex)
			: std::unique_lock<std::mutex>();

	assert(state == State::STARTED);

	if (functions.restart) {
		logger->debug("Executing Lua function: restart()");
		lua_pushvalue(L, functions.restart);
		int ret = lua_pcall(L, 0, 0, 0);
		if (ret)
			throw LuaError(L, ret);
	}
	else
		Hook::restart();
}

void
LuaHook::periodic()
{
	assert(state == State::STARTED);

	if (functions.periodic) {
		auto lockScope = needsLocking
			? std::unique_lock<std::mutex>(mutex)
			: std::unique_lock<std::mutex>();

		logger->debug("Executing Lua function: restart()");
		lua_pushvalue(L, functions.periodic);
		int ret = lua_pcall(L, 0, 0, 0);
		if (ret)
			throw LuaError(L, ret);
	}
}

Hook::Reason
LuaHook::process(sample *smp)
{
	if (!functions.process && !hasExpressions)
		return Reason::OK;

	int rtype;
	enum Reason reason;
	auto lockScope = needsLocking
			? std::unique_lock<std::mutex>(mutex)
			: std::unique_lock<std::mutex>();

	/* First, run the process() function of the script */
	if (functions.process) {
		logger->debug("Executing Lua function: process(smp)");

		lua_pushsample(L, smp, useNames);

		lua_pushvalue(L, functions.process);
		lua_pushvalue(L, -2); // Push a copy since lua_pcall() will pop it
		int ret = lua_pcall(L, 1, 1, 0);
		if (ret)
			throw LuaError(L, ret);

		rtype = lua_type(L, -1);
		if (rtype == LUA_TNUMBER) {
			reason = (Reason) lua_tonumber(L, -1);
		}
		else {
			logger->warn("Lua process() did not return a valid number. Assuming Reason::OK");
			reason = Reason::OK;
		}

		lua_pop(L, 1);

		lua_tosample(L, smp, &signalsProcessed, useNames);
	}
	else
		reason = Reason::OK;

	/* After that evaluate expressions */
	if (hasExpressions) {
		lua_pushsample(L, smp, useNames);
		lua_setglobal(L, "smp");

		for (unsigned i = 0; i < expressions.size(); i++) {
			auto *sig = (struct signal *) vlist_at(&signalsExpressions, i);

			expressions[i].evaluate(&smp->data[i], sig->type);
		}

		smp->length = expressions.size();
	}

	return reason;
}

/* Register hook */
static char n[] = "lua";
static char d[] = "Implement hook in Lua";
static HookPlugin<LuaHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH, 1> p;

} /* namespace node */
} /* namespace villas */

/** @} */
