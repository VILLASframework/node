/** Redis node-type
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

#include <chrono>

#include <sys/time.h>

#include <villas/nodes/redis.hpp>
#include <villas/utils.hpp>
#include <villas/sample.h>
#include <villas/exceptions.hpp>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>
#include <villas/timing.h>

/* Forward declartions */
static struct vnode_type p;

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace sw::redis;

static void redis_on_message(struct vnode *n, std::string channel, std::string msg)
{
	struct redis *r = (struct redis *) n->_vd;

	// n->logger->info("Message: {}: {}", channel, msg);

	switch (r->mode) {
		case RedisMode::CHANNEL: {
			int alloc, scanned, pushed;
			unsigned cnt = n->in.vectorize;
			struct sample *smps[cnt];
			size_t rbytes;

			alloc = sample_alloc_many(&r->pool, smps, cnt);
			if (alloc < 0)
				return;

			scanned = r->formatter->sscan(msg.c_str(), msg.size(), &rbytes, smps, alloc);
			if (scanned < 0)
				return;

			pushed = queue_push_many(&r->queue, (void **) smps, scanned);
			if (pushed < 0)
				return;

			sample_decref_many(smps + pushed, alloc - pushed);
			break;
		}

		case RedisMode::HASH:
			if (msg == "hset")
				r->updated = true;
			break;

		case RedisMode::KEY:
			if (msg == "set")
				r->updated = true;
			break;

		case RedisMode::STREAM:
			throw NotImplementedError();
	}
}

int redis_init(struct vnode *n)
{
	struct redis *r = (struct redis *) n->_vd;

	r->mode = RedisMode::KEY;
	r->context = nullptr;
	r->formatter = nullptr;
	r->notify = true;
	r->signals = true;
	r->rate = 1.0;
	r->updated = false;

	new (&r->options) ConnectionOptions;
	new (&r->pool_options) ConnectionPoolOptions;
	new (&r->task) Task(CLOCK_REALTIME);
	new (&r->uri) std::string();
	new (&r->prefix) std::string();

	return 0;
}

int redis_destroy(struct vnode *n)
{
	int ret;
	struct redis *r = (struct redis *) n->_vd;

	if (r->context)
		delete r->context;

	if (r->formatter)
		delete r->formatter;

	using string = std::string;

	r->options.~ConnectionOptions();
	r->pool_options.~ConnectionPoolOptions();
	r->uri.~string();
	r->prefix.~string();
	r->task.~Task();

	ret = queue_destroy(&r->queue);
	if (ret)
		return ret;

	ret = pool_destroy(&r->pool);
	if (ret)
		return ret;

	return 0;
}

int redis_parse(struct vnode *n, json_t *json)
{
	int ret;
	struct redis *r = (struct redis *) n->_vd;

	json_error_t err;
	json_t *json_ssl = nullptr;
	json_t *json_format = nullptr;

	const char *host = nullptr;
	const char *path = nullptr;
	const char *user = nullptr;
	const char *password = nullptr;
	const char *mode = nullptr;
	const char *uri = nullptr;
	const char *prefix = nullptr;
	int keepalive = -1;
	int db = -1;
	int notify = -1;
	int signals = -1;

	double connect_timeout = -1;
	double socket_timeout = -1;

	ret = json_unpack_ex(json, &err, 0, "{ s?: o, s?: s, s?: s, s?: i, s?: s, s?: s, s?: s, s?: i, s?: { s?: F, s?: F }, s?: o, s?: b, s?: s, s?: b, s?: s, s?: b, s?: F }",
		"format", &json_format,
		"uri", &uri,
		"host", &host,
		"port", &r->options.port,
		"path", &path,
		"user", &user,
		"password", &password,
		"db", &db,
		"timeout",
			"connect", &connect_timeout,
			"socket", &socket_timeout,
		"ssl", &json_ssl,
		"keepalive", &keepalive,
		"mode", &mode,
		"signals", &signals,
		"prefix", &prefix,
		"notify", &notify,
		"rate", &r->rate
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-redis", "Failed to parse node configuration");

	if (json_ssl) {
		const char *cacert;
		const char *cacertdir;
		const char *cert;
		const char *key;

		int enabled = 1;

		ret = json_unpack_ex(json_ssl, &err, 0, "{ s?: b, s?: s, s?: s, s?: s, s?: s }",
			"enabled", &enabled,
			"cacert", &cacert,
			"cacertdir", &cacertdir,
			"cert", &cert,
			"key", &key
		);
		if (ret)
			throw ConfigError(json, err, "node-config-node-redis-ssl", "Failed to parse Redis SSL configuration");

		r->options.tls.enabled = enabled != 0;

		r->options.tls.cacert = cacert;
		r->options.tls.cacertdir = cacertdir;
		r->options.tls.cert = cert;
		r->options.tls.key = key;

		if (host)
			r->options.tls.sni = host;
	}

	/* Mode */
	if (mode) {
		if (!strcmp(mode, "key") || !strcmp(mode, "set-get"))
			r->mode = RedisMode::KEY;
		else if (!strcmp(mode, "hash") || !strcmp(mode, "hset-hget"))
			r->mode = RedisMode::HASH;
		else if (!strcmp(mode, "channel") || !strcmp(mode, "pub-sub"))
			r->mode = RedisMode::CHANNEL;
		else if (!strcmp(mode, "stream"))
			r->mode = RedisMode::STREAM;
		else
			throw ConfigError(json, "node-config-node-redis-mode", "Invalid Redis mode: {}", mode);
	}

	/* Format */
	r->formatter = json_format
			? FormatFactory::make(json_format)
			: FormatFactory::make("json");
	if (!r->formatter)
		throw ConfigError(json_format, "node-config-node-redis-format", "Invalid format configuration");

	if (db >= 0)
		r->options.db = db;

	if (uri)
		r->uri = uri;

	if (prefix)
		r->prefix = prefix;

	if (user)
		r->options.user = user;

	if (password)
		r->options.password = password;

	if (host)
		r->options.host = host;

	if (path)
		r->options.path = path;

	if (keepalive >= 0)
		r->options.keep_alive = keepalive != 0;

	if (notify >= 0)
		r->notify = notify != 0;

	if (socket_timeout >= 0)
		r->options.socket_timeout = std::chrono::milliseconds((int) (1000 * socket_timeout));

	if (connect_timeout >= 0)
		r->options.connect_timeout = std::chrono::milliseconds((int) (1000 * connect_timeout));

	return 0;
}

int redis_check(struct vnode *n)
{
	struct redis *r = (struct redis *) n->_vd;

	if (!r->uri.empty() && (!r->options.host.empty() || !r->options.path.empty()))
		return -1;

	if (!r->uri.empty() && r->options.host.empty() && r->options.path.empty())
		return -1;

	if (!r->options.host.empty() && !r->options.path.empty())
		return -1;

	if (r->mode == RedisMode::HASH && r->signals == false)
		return -1;

	if (r->options.db < 0)
		return -1;

	return 0;
}

char * redis_print(struct vnode *n)
{
	struct redis *r = (struct redis *) n->_vd;

	const char *mode;

	switch (r->mode) {
		case RedisMode::KEY:
			mode = "key";
			break;

		case RedisMode::HASH:
			mode = "hash";
			break;

		case RedisMode::CHANNEL:
			mode = "channel";
			break;

		case RedisMode::STREAM:
			mode = "stream";
			break;
	}

	return strf("mode=%s, prefix=%s, host=%s, port=%d, path=%s, user=%s, ssl.enabled=%s, notify=%s, notify_signal=%s, signals=%s, rate=%f", mode,
		r->prefix.c_str(),
		r->options.host.c_str(),
		r->options.port,
		r->options.path.c_str(),
		r->options.user.c_str(),
		r->options.tls.enabled ? "yes" : "no",
		r->notify ? "yes" : "no",
		r->notify_signal.c_str(),
		r->signals ? "yes" : "no",
		r->rate
	);
}

int redis_prepare(struct vnode *n)
{
	int ret;
	struct redis *r = (struct redis *) n->_vd;

	r->options.type = r->options.path.empty()
		? ConnectionType::TCP
		: ConnectionType::UNIX;

	if (r->prefix.empty())
		r->prefix = node_name_short(n);

	if (r->notify_signal.empty()) {
		struct signal *first_sig = (struct signal *) vlist_first(&n->in.signals);
		r->notify_signal = first_sig->name;
	}

	ret = queue_init(&r->queue, 1024);
	if (ret)
		return ret;

	ret = pool_init(&r->pool, 1024, SAMPLE_LENGTH(vlist_length(&n->in.signals)));
	if (ret)
		return ret;

	return 0;
}

int redis_start(struct vnode *n)
{
	struct redis *r = (struct redis *) n->_vd;

	r->formatter->start(&n->in.signals, ~(int) SampleFlags::HAS_OFFSET);

	/* Conenct to Redis server */
	r->context = r->uri.empty()
		? new Redis(r->options, r->pool_options)
		: new Redis(r->uri);

	if (r->notify || r->mode == RedisMode::CHANNEL) {
		new (&r->subscriber) Subscriber(r->context->subscriber());

		r->subscriber.on_message([n](std::string channel, std::string msg) {
			redis_on_message(n, channel, msg);
		});
		r->subscriber.on_pmessage([n](std::string pattern, std::string channel, std::string msg) {
			redis_on_message(n, channel, msg);
		});
	}

	if (r->notify) {
		/* Enable keyspace notifications */
		r->context->command("config", "set", "notify-keyspace-events", "K$h");
	}
	else {
		r->task.setRate(r->rate);
	}

	switch (r->mode) {
		case RedisMode::CHANNEL:
			if (r->signals) {
				auto pattern = fmt::format("{}.{}", r->prefix, r->notify_signal);
				r->subscriber.psubscribe(pattern);
			}
			else
				r->subscriber.subscribe(r->prefix);
			break;

		case RedisMode::HASH:
			if (r->notify) {
				auto pattern = fmt::format("__keyspace@{}__:{}", r->options.db, r->prefix);
				r->subscriber.psubscribe(pattern);
			}
			break;

		case RedisMode::KEY:
			if (r->notify) {
				if (r->signals) {
					auto pattern = fmt::format("__keyspace@{}__:{}.{}", r->options.db, r->prefix, r->notify_signal);
					r->subscriber.psubscribe(pattern);
				}
				else {
					auto pattern = fmt::format("__keyspace@{}__:{}", r->options.db, r->prefix);
					r->subscriber.subscribe(r->prefix);
				}
			}
			break;

		case RedisMode::STREAM: {}
	}

	return 0;
}

int redis_stop(struct vnode *n)
{
	struct redis *r = (struct redis *) n->_vd;

	r->task.stop();

	if (r->notify || r->mode == RedisMode::CHANNEL)
		r->subscriber.~Subscriber();

	if (r->context) {
		delete r->context;
		r->context = nullptr;
	}

	return 0;
}

int redis_read(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	int ret;
	struct redis *r = (struct redis *) n->_vd;

	/* Wait for update */
	if (r->mode != RedisMode::CHANNEL) {
		if (r->notify) {
			/* Wait for keyspace event */
			while (!r->updated)
				r->subscriber.consume();

			r->updated = false;
		}
		else {
			if (r->task.wait() == 0)
				throw SystemError("Failed to wait for task");
		}
	}

	switch (r->mode) {
		case RedisMode::CHANNEL:
			if (r->signals) {


			}
			else {
				int pulled_cnt;
				struct sample *pulled_smps[cnt];

				pulled_cnt = queue_pull_many(&r->queue, (void **) pulled_smps, cnt);

				sample_copy_many(smps, pulled_smps, pulled_cnt);
				sample_decref_many(pulled_smps, pulled_cnt);

				return pulled_cnt;
			}
			break;

		case RedisMode::KEY:
			if (r->signals) {
				struct sample *smp = smps[0];

				std::vector<std::string> keys;

				for (unsigned j = 0; j < vlist_length(&n->in.signals); j++) {
					struct signal *sig = (struct signal *) vlist_at(&n->in.signals, j);
					union signal_data *data = &smp->data[j];

					auto key = fmt::format("{}.{}", r->prefix, sig->name);

					*data = sig->init;

					keys.push_back(key);
				}

				std::vector<std::optional<std::string>> replies;
				r->context->mget(keys.begin(), keys.end(), std::back_inserter(replies));

				unsigned j = 0;
				for (auto it = replies.begin(); it != replies.end(); ++it, j++) {
					struct signal *sig = (struct signal *) vlist_at(&n->in.signals, j);
					union signal_data *data = &smp->data[j];

					auto &value = *it;
					if (value) {
						char *end;
						ret = signal_data_parse_str(data, sig->type, value->c_str(), &end);
						if (ret)
							return -1;
					}
				}

				smp->flags = 0;
				smp->length = vlist_length(&n->in.signals);

				if (smp->length > 0)
					smp->flags |= (int) SampleFlags::HAS_DATA;

				return 1;
			}
			else {
				auto value = r->context->get(r->prefix);
				if (value) {
					size_t rbytes, bytes = value->size();
					ret = r->formatter->sscan(value->c_str(), bytes, &rbytes, smps, cnt);

					if (rbytes != bytes)
						return -1;

					return ret;
				}
				else
					return -1;
			}
			break;

		case RedisMode::HASH: {
			struct sample *smp = smps[0];

			for (unsigned j = 0; j < vlist_length(&n->in.signals); j++) {
				struct signal *sig = (struct signal *) vlist_at(&n->in.signals, j);
				union signal_data *data = &smp->data[j];

				*data = sig->init;
			}

			std::unordered_map<std::string, std::string> kvs;
			r->context->hgetall(r->prefix, std::inserter(kvs, kvs.begin()));

			for (auto &it : kvs) {
				auto &name = it.first;
				auto &value = it.second;

				struct signal *sig = vlist_lookup_name<struct signal>(&n->in.signals, name);
				if (!sig)
					continue;

				auto i = vlist_index(&n->in.signals, sig);

				char *end;
				ret = signal_data_parse_str(&smp->data[i], sig->type, value.c_str(), &end);
				if (ret < 0)
					continue;
			}

			smp->flags = 0;
			smp->length = vlist_length(&n->in.signals);

			if (smp->length > 0)
				smp->flags |= (int) SampleFlags::HAS_DATA;

			return 1;
		}

		case RedisMode::STREAM:
			throw NotImplementedError();
	}

	return 0;
}

int redis_write(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	int ret;
	struct redis *r = (struct redis *) n->_vd;

	switch (r->mode) {
		case RedisMode::CHANNEL:
			for (unsigned i = 0; i < cnt; i++) {
				struct sample *smp = smps[i];

				if (r->signals) {
					for (unsigned j = 0; j < MIN(vlist_length(smp->signals), smp->length); j++) {
						struct signal *sig = (struct signal *) vlist_at(smp->signals, j);
						union signal_data *data = &smp->data[j];

						char val[128];
						signal_data_print_str(data, sig->type, val, sizeof(val), 16);

						auto channel = fmt::format("{}.{}", r->prefix, sig->name);
						auto value = std::string_view(val);

						r->context->publish(channel, value);
					}
				}
				else {
					char buf[1500];
					size_t wbytes;

					ret = r->formatter->sprint(buf, sizeof(buf), &wbytes, smps, cnt);
					if (ret < 0)
						return ret;

					auto value = std::string_view(buf, wbytes);

					r->context->publish(r->prefix, value);
				}
			}
			break;

		case RedisMode::KEY:
			if (r->signals) {
				/* We only update the signals with their latest value here. */
				struct sample *smp = smps[cnt - 1];

				std::vector<std::pair<std::string, std::string>> kvs;

				for (unsigned j = 0; j < MIN(vlist_length(smp->signals), smp->length); j++) {
					struct signal *sig = (struct signal *) vlist_at(smp->signals, j);
					union signal_data *data = &smp->data[j];

					char val[128];
					signal_data_print_str(data, sig->type, val, sizeof(val), 16);

					auto key = fmt::format("{}.{}", r->prefix, sig->name);
					auto value = std::string_view(val);

					kvs.emplace_back(key, value);
				}

				r->context->mset(kvs.begin(), kvs.end());
			}
			else {
				char buf[1500];
				size_t wbytes;

				ret = r->formatter->sprint(buf, sizeof(buf), &wbytes, smps, cnt);
				if (ret < 0)
					return ret;

				auto value = std::string_view(buf, wbytes);

				r->context->set(r->prefix, value);
			}
			break;

		case RedisMode::HASH: {
			/* We only update the signals with their latest value here. */
			struct sample *smp = smps[cnt - 1];

			std::unordered_map<std::string, std::string> kvs;

			for (unsigned j = 0; j < MIN(vlist_length(smp->signals), smp->length); j++) {
				struct signal *sig = (struct signal *) vlist_at(smp->signals, j);
				union signal_data *data = &smp->data[j];

				char val[128];
				signal_data_print_str(data, sig->type, val, sizeof(val), 16);

				kvs[sig->name] = val;
			}

			r->context->hmset(r->prefix, kvs.begin(), kvs.end());
			break;
		}

		case RedisMode::STREAM:
			throw NotImplementedError();
	}

	return cnt;
}

int redis_poll_fds(struct vnode *n, int fds[])
{
	struct redis *r = (struct redis *) n->_vd;

	if (r->notify)
		return 0;

	fds[0] = r->task.getFD();

	return 1;
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "redis";
	p.description	= "Redis key-value store";
	p.vectorize	= 0;
	p.size		= sizeof(struct redis);
	p.init		= redis_init;
	p.destroy	= redis_destroy;
	p.parse		= redis_parse;
	p.check		= redis_check;
	p.print		= redis_print;
	p.prepare	= redis_prepare;
	p.start		= redis_start;
	p.stop		= redis_stop;
	p.read		= redis_read;
	p.write		= redis_write;
	p.poll_fds	= redis_poll_fds;

	if (!node_types)
		node_types = new NodeTypeList();

	node_types->push_back(&p);
}
