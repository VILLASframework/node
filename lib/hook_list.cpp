/** Hook-releated functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/plugin.hpp>
#include <villas/hook.hpp>
#include <villas/hook_list.hpp>
#include <villas/list.hpp>
#include <villas/utils.hpp>
#include <villas/sample.hpp>

using namespace villas;
using namespace villas::node;

void HookList::parse(json_t *json, int mask, Path *o, Node *n)
{
	if (!json_is_array(json))
		throw ConfigError(json, "node-config-hook", "Hooks must be configured as a list of hook objects");

	size_t i;
	json_t *json_hook;
	json_array_foreach(json, i, json_hook) {
		int ret;
		const char *type;
		Hook::Ptr h;
		json_error_t err;
		json_t *json_config;

		switch (json_typeof(json_hook)) {
			case JSON_STRING:
				type = json_string_value(json_hook);
				json_config = json_object();
				break;

			case JSON_OBJECT:
				ret = json_unpack_ex(json_hook, &err, 0, "{ s: s }", "type", &type);
				if (ret)
					throw ConfigError(json_hook, err, "node-config-hook", "Failed to parse hook");
				json_config = json_hook;
				break;

			default:
				throw ConfigError(json_hook, "node-config-hook", "Hook must be configured by simple string or object");
		}

		auto hf = plugin::registry->lookup<HookFactory>(type);
		if (!hf)
			throw ConfigError(json_hook, "node-config-hook", "Unknown hook type '{}'", type);

		if (!(hf->getFlags() & mask))
			throw ConfigError(json_hook, "node-config-hook", "Hook '{}' not allowed here", type);

		h = hf->make(o, n);
		h->parse(json_config);

		push_back(h);
	}
}

void HookList::check()
{
	for (auto h : *this)
		h->check();
}

void HookList::prepare(SignalList::Ptr signals, int m, Path *p, Node *n)
{
	if (!m)
		goto skip_add;

	/* Add internal hooks if they are not already in the list */
	for (auto f : plugin::registry->lookup<HookFactory>()) {
		if ((f->getFlags() & m) == m) {
			auto h = f->make(p, n);
			push_back(h);
		}
	}

skip_add:
	/* Remove filters which are not enabled */
	remove_if([](Hook::Ptr h) { return !h->isEnabled(); });

	/* We sort the hooks according to their priority */
	sort([](const value_type &a, const value_type b) { return a->getPriority() < b->getPriority(); });

	unsigned i = 0;
	auto sigs = signals;
	for (auto h : *this) {
		h->prepare(sigs);

		sigs = h->getSignals();

		auto logger = h->getLogger();
		logger->debug("Signal list after hook #{}:", i++);
		if (logger->level() <= spdlog::level::debug)
			sigs->dump(logger);
	}
}

int HookList::process(struct Sample * smps[], unsigned cnt)
{
	unsigned current, processed = 0;

	if (size() == 0)
		return cnt;

	for (current = 0; current < cnt; current++) {
		struct Sample *smp = smps[current];

		for (auto h : *this) {
			auto ret = h->process(smp);
			smp->signals = h->getSignals();
			switch (ret) {
				case Hook::Reason::ERROR:
					return -1;

				case Hook::Reason::OK:
					continue;

				case Hook::Reason::SKIP_SAMPLE:
					goto skip;

				case Hook::Reason::STOP_PROCESSING:
					goto stop;
			}
		}

stop:	SWAP(smps[processed], smps[current]);
		processed++;
skip: {}
	}

		return processed;
}

void HookList::periodic()
{
	for (auto h : *this)
		h->periodic();
}

void HookList::start()
{
	for (auto h : *this)
		h->start();
}

void HookList::stop()
{
	for (auto h : *this)
		h->stop();
}

SignalList::Ptr HookList::getSignals() const
{
	auto h = back();
	if (!h)
		return nullptr;

	return h->getSignals();
}

unsigned HookList::getSignalsMaxCount() const
{
	unsigned max_cnt = 0;

	for (auto h : *this) {
		unsigned sigs_cnt = h->getSignals()->size();

		if (sigs_cnt > max_cnt)
			max_cnt = sigs_cnt;
	}

	return max_cnt;
}

json_t * HookList::toJson() const
{
	json_t *json_hooks = json_array();

	for (auto h : *this)
		json_array_append(json_hooks, h->getConfig());

	return json_hooks;
}

void HookList::dump(Logger logger, std::string subject) const
{
	logger->debug("Hooks of {}:", subject);

	unsigned i = 0;
	for (auto h : *this)
		logger->debug("      {}: {}", i++, h->getFactory()->getName());
}
