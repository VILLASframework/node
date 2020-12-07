
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

#include <lua.h>

#include <villas/hook.hpp>
#include <villas/node.h>
#include <villas/sample.h>

namespace villas {
namespace node {

class LuaHook : public Hook {

protected:
	std::string script;
	lua_State *L;

	/* Function indizes */
	struct {
		int start;
		int stop;
		int process;
		int periodic;
		int prepare;
	} function;

	void sampleToLua(struct sample *smp, int idx = -1)
	{
		for (unsigned i = 0; i < smp->length; i++) {
			struct signal_type *sig = (struct signal_type *) vlist_at(smp->signals, i);
			struct signal_data *data = &smp->data[i];

			lua_pushnumber(L, i);   /* Push the table index */

			switch (sig->type) {
				case FLOAT:
					lua_pushnumber(L, data->f);
					break;

				case INTEGER:
					lua_pushinteger(L, data->i);
					break;

				case BOOLEAN:
					lua_pushboolean(L, data->b);
					break;

				case COMPLEX:
					/* Not supported yet */
				case INVALID:
				default:
					return Reason::ERROR;
			}


			lua_rawset(L, idx - 2);
		}
	}

	void luaToSample(struct sample *smp, int idx = -1)
	{
		int ret;

		for (smp->length = 0; i < MAX(vlist_length(&signals), smp->capacity); i++) {
			struct signal_type *sig = (struct signal_type *) vlist_at(&signals, i);
			ret = lua_rawgeti(L, idx, i);

			switch (ret) {
				case LUA_TBOOLEAN:
					break;

				case LUA_TNUMBER:
					break;

				default:
					continue;
			}
		}
	}

	void parseSignals(json_t *json_sigs)
	{
		size_t i;
		json_t *json_sig;

		if (!json_is_array(json_sigs))
			throw ConfigError(json_sigs, "node-config-hook-average-signals", "Setting 'signals' must be a list of dicts");

		json_array_foreach(json_sigs, i, json_sig)
			parseSignal(json_sig);
	}

	void parseSignal(json_t *json_sig)
	{
		switch (json_typeof(json_signal)) {
			case JSON_STRING:
				vlist_push(&signal_names, strdup(json_string_value(json_signal)));
				break;

			case JSON_INTEGER:
				mask.set(json_integer_value(json_signal));
				break;

			default:
				throw ConfigError(json_signal, "node-config-hook-average-signals", "Invalid value for setting 'signals'");
		}
	}

public:
	Hook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true)
	{
		L = luaL_newstate();
	}

	virtual ~Hook()
	{
		lua_close(L);
	}

	virtual void parse(json_t *c)
	{
		int ret;
		size_t i;
		const char *script_str;
		json_error_t err;
		json_t *json_signals = nullptr;

		assert(state != State::STARTED);

		Hook::parse(cfg);

		ret = json_unpack_ex(cfg, &err, 0, "{ s: s, s?: o }",
			"script", &script_str,
			"signals", &json_signals
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-average");

		script = script_str;

		if (json_signals)
			parseSignals(json_signals);

		state = State::PARSED;
	}

	virtual void check()
	{
		assert(state == State::PARSED);

		state = State::CHECKED;
	}

	void prepare(struct vlist *sigs)
	{
		int ret;

		/* Load Lua standard libraries */
		luaL_openlibs(L);

		/* Load our Lua script */
		luaL_dofile(script.c_str())

		/* Lookup functions */
		std::map<const char *, int *> funcs = {
			{ "start",    &function.start    },
			{ "stop",     &function.stop     },
			{ "prepare",  &function.prepare  },
			{ "periodic", &function.periodic },
			{ "process",  &function.process  }
		};

		for (auto it : funcs) {
			ret = lua_getglobal(L, it->first);
			if (ret == LUA_TFUNCTION) {
				*(it->second) = lua_gettop(L);
			}
			else {
				*(it->second) = 0;
				lua_pop(L, 1);
			}
		}

		ret = signal_list_copy(&signals, sigs);
		if (ret)
			throw RuntimeError("Failed to copy signal list");

		prepare();
	}

	/** Called whenever a hook is started; before threads are created. */
	virtual void start()
	{
		int ret;
		assert(state == State::PREPARED);

		if (functions.start) {
			lua_pushvalue(L, functions.start);
			lua_call(L, 0, 0);
		}

		state = State::STARTED;
	}

	/** Called whenever a hook is stopped; after threads are destoyed. */
	virtual void stop()
	{
		assert(state == State::STARTED);

		if (functions.stop) {
			lua_pushvalue(L, functions.stop);
			lua_call(L, 0, 0);
		}

		state = State::STOPPED;
	}

	/** Called whenever a new simulation case is started. This is detected by a sequence no equal to zero. */
	virtual void restart()
	{
		assert(state == State::STARTED);

		if (functions.restart) {
			lua_pushvalue(L, functions.restart);
			lua_call(L, 0, 0);
		}
		else
			Hook::restart();
	}

	/** Called whenever a sample is processed. */
	virtual Reason process(sample *smp)
	{
		lua_newtable(L);

		if (functions.process) {
			lua_pushvalue(L, functions.process);
			sampleToLua(smp);

			lua_call(L, 1, 1);

			luaToSample(smp);
		}

		return Reason::OK;
	};
};

/* Register hook */
static char n[] = "lua";
static char d[] = "Implement hook in Lua";
static HookPlugin<LuaHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH, 1> p;

} /* namespace node */
} /* namespace villas */

/** @} */
