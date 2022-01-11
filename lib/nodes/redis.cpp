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

#include <functional>
#include <chrono>
#include <unordered_map>

#include <sys/time.h>

#include <villas/nodes/redis.hpp>
#include <villas/nodes/redis_helpers.hpp>
#include <villas/utils.hpp>
#include <villas/sample.hpp>
#include <villas/exceptions.hpp>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>
#include <villas/timing.hpp>
#include <villas/node/config.hpp>
#include <villas/node_compat.hpp>
#include <villas/node_compat_type.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

/* Forward declartions */
static NodeCompatType p;
static void redis_on_message(NodeCompat *n, const std::string &channel, const std::string &msg);

static std::unordered_map<sw::redis::ConnectionOptions, RedisConnection*> connections;

RedisConnection::RedisConnection(const sw::redis::ConnectionOptions &opts) :
	context(opts),
	subscriber(context.subscriber()),
	logger(logging.get("nodes:redis"))
{
	/* Enable keyspace notifications */
	context.command("config", "set", "notify-keyspace-events", "K$h");

	subscriber.on_message([this](const std::string &channel, const std::string &msg) {
		onMessage(channel, msg);
	});

	logger->info("New connection: {}", opts);

	state = State::INITIALIZED;
}

RedisConnection * RedisConnection::get(const sw::redis::ConnectionOptions &opts)
{
	RedisConnection *conn;

	auto it = connections.find(opts);
	if (it != connections.end())
		conn = it->second;
	else {
		try {
			conn = new RedisConnection(opts);
		} catch (sw::redis::IoError &e) {
			throw RuntimeError(e.what());
		}

		connections[opts] = conn;
	}

	return conn;
}

void RedisConnection::onMessage(const std::string &channel, const std::string &msg)
{
	auto itp = subscriberMap.equal_range(channel);
	for (auto it = itp.first; it != itp.second; ++it) {
		NodeCompat *n = it->second;

		redis_on_message(n, channel, msg);
	}
}

void RedisConnection::subscribe(NodeCompat *n, const std::string &channel)
{
	subscriber.subscribe(channel);

	subscriberMap.emplace(channel, n);
}

void RedisConnection::unsubscribe(NodeCompat *n, const std::string &channel)
{
	auto itp = subscriberMap.equal_range(channel);
	for (auto it = itp.first; it != itp.second; ++it) {
		if (it->second == n)
			subscriberMap.erase(it);
	}

	if (subscriberMap.count(channel) == 0)
		subscriber.subscribe(channel);
}

void RedisConnection::start()
{
	if (state == State::RUNNING)
		return; /* Already running */

	state = State::RUNNING;

	thread = std::thread(&RedisConnection::loop, this);
}

void RedisConnection::stop()
{
	state = State::STOPPING;

	thread.join();

	state = State::INITIALIZED;
}

void RedisConnection::loop()
{
	while (state == State::RUNNING) {
		try {
			subscriber.consume();
		}
		catch (const sw::redis::TimeoutError &e) {
			continue;
		}
		catch (const sw::redis::ReplyError &e) {
			continue;
		}
		catch (const sw::redis::Error &e) {
			/* Create a new subscriber */
			subscriber = context.subscriber();
		}
	}
}

static
int redis_get(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int ret;
	auto *r = n->getData<struct redis>();

	switch (r->mode) {
		case RedisMode::KEY: {
			auto value = r->conn->context.get(r->key);
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

		case RedisMode::HASH: {
			struct Sample *smp = smps[0];

			for (unsigned j = 0; j < n->getInputSignals(false)->size(); j++) {
				auto sig = n->getInputSignals(false)->getByIndex(j);
				auto *data = &smp->data[j];

				*data = sig->init;
			}

			std::unordered_map<std::string, std::string> kvs;
			r->conn->context.hgetall(r->key, std::inserter(kvs, kvs.begin()));

			int max_idx = -1;
			for (auto &it : kvs) {
				auto &name = it.first;
				auto &value = it.second;

				auto sig = n->getInputSignals(false)->getByName(name);
				if (!sig)
					continue;

				auto idx = n->getInputSignals(false)->getIndexByName(name);
				if (idx > max_idx)
					max_idx = idx;

				char *end;
				ret = smp->data[idx].parseString(sig->type, value.c_str(), &end);
				if (ret < 0)
					continue;
			}

			smp->flags = 0;
			smp->length = max_idx + 1;

			if (smp->length > 0)
				smp->flags |= (int) SampleFlags::HAS_DATA;

			return 1;
		}

		default:
			return -1;
	}
}

static
void redis_on_message(NodeCompat *n, const std::string &channel, const std::string &msg)
{
	auto *r = n->getData<struct redis>();

	n->logger->debug("Message: {}: {}", channel, msg);

	int alloc, scanned, pushed;
	unsigned cnt = n->in.vectorize;
	struct Sample *smps[cnt];

	alloc = sample_alloc_many(&r->pool, smps, cnt);
	if (alloc < 0) {
		n->logger->error("Failed to allocate samples");
		return;
	}
	else if ((unsigned) alloc < cnt)
		n->logger->warn("Pool underrun");

	switch (r->mode) {
		case RedisMode::CHANNEL: {
			size_t rbytes;
			scanned = r->formatter->sscan(msg.c_str(), msg.size(), &rbytes, smps, alloc);
			break;
		}

		case RedisMode::HASH:
		case RedisMode::KEY:
			scanned = redis_get(n, smps, cnt);
			break;

		default:
			goto out;
	}

	if (scanned < 0) {
		n->logger->error("Failed to decode samples");
		pushed = 0;
		goto out;
	}

	pushed = queue_signalled_push_many(&r->queue, (void **) smps, scanned);
	if (pushed < 0) {
		n->logger->error("Failed to enqueue");
		pushed = 0;
	}
	else if (pushed != scanned)
		n->logger->warn("Queue underrun");

out:	sample_decref_many(smps + pushed, alloc - pushed);
}

int villas::node::redis_init(NodeCompat *n)
{
	auto *r = n->getData<struct redis>();

	r->mode = RedisMode::KEY;
	r->formatter = nullptr;
	r->notify = true;
	r->rate = 1.0;

	new (&r->options) sw::redis::ConnectionOptions;
	new (&r->task) Task(CLOCK_REALTIME);
	new (&r->key) std::string();

	/* We need a timeout in order for RedisConnection::loop() to properly
	 * terminate after the node is stopped */
	r->options.socket_timeout = std::chrono::milliseconds(500);

	return 0;
}

int villas::node::redis_destroy(NodeCompat *n)
{
	int ret;
	auto *r = n->getData<struct redis>();

	if (r->formatter)
		delete r->formatter;

	using string = std::string;
	using redis_co = sw::redis::ConnectionOptions;

	r->options.~redis_co();
	r->key.~string();
	r->task.~Task();

	ret = queue_signalled_destroy(&r->queue);
	if (ret)
		return ret;

	ret = pool_destroy(&r->pool);
	if (ret)
		return ret;

	return 0;
}

int villas::node::redis_parse(NodeCompat *n, json_t *json)
{
	int ret;
	auto *r = n->getData<struct redis>();

	json_error_t err;
	json_t *json_ssl = nullptr;
	json_t *json_format = nullptr;

	const char *host = nullptr;
	const char *path = nullptr;
	const char *user = nullptr;
	const char *password = nullptr;
	const char *mode = nullptr;
	const char *uri = nullptr;
	const char *key = nullptr;
	const char *channel = nullptr;
	int keepalive = -1;
	int db = -1;
	int notify = -1;

	double connect_timeout = -1;
	double socket_timeout = -1;

	ret = json_unpack_ex(json, &err, 0, "{ s?: o, s?: s, s?: s, s?: i, s?: s, s?: s, s?: s, s?: i, s?: { s?: F, s?: F }, s?: o, s?: b, s?: s, s?: s, s?: s, s?: b, s?: F }",
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
		"key", &key,
		"channel", &channel,
		"notify", &notify,
		"rate", &r->rate
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-redis", "Failed to parse node configuration");

	if (json_ssl) {
#ifdef REDISPP_WITH_TLS
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
#else
		throw ConfigError(json_ssl, "node-config-node-redis-ssl", "This built of the redis++ library does not support SSL");
#endif /* REDISPP_WITH_TLS */
	}

	/* Mode */
	if (mode) {
		if (!strcmp(mode, "key") || !strcmp(mode, "set-get"))
			r->mode = RedisMode::KEY;
		else if (!strcmp(mode, "hash") || !strcmp(mode, "hset-hget"))
			r->mode = RedisMode::HASH;
		else if (!strcmp(mode, "channel") || !strcmp(mode, "pub-sub"))
			r->mode = RedisMode::CHANNEL;
		else
			throw ConfigError(json, "node-config-node-redis-mode", "Invalid Redis mode: {}", mode);
	}

	/* Format */
	if (r->formatter)
		delete r->formatter;
	r->formatter = json_format
			? FormatFactory::make(json_format)
			: FormatFactory::make("json");
	if (!r->formatter)
		throw ConfigError(json_format, "node-config-node-redis-format", "Invalid format configuration");

	if (key && r->mode == RedisMode::KEY)
		r->key = key;
	if (channel && r->mode == RedisMode::CHANNEL)
		r->key = channel;

	if (notify >= 0)
		r->notify = notify != 0;

	/* Connection options */
	if (uri)
		r->options = sw::redis::ConnectionOptions(uri);

	if (db >= 0)
		r->options.db = db;

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

	if (socket_timeout > 0)
		r->options.socket_timeout = std::chrono::milliseconds((int) (1000 * socket_timeout));

	if (connect_timeout > 0)
		r->options.connect_timeout = std::chrono::milliseconds((int) (1000 * connect_timeout));

	return 0;
}

int villas::node::redis_check(NodeCompat *n)
{
	auto *r = n->getData<struct redis>();

	if (!r->options.host.empty() && !r->options.path.empty())
		return -1;

	if (r->options.db < 0)
		return -1;

	return 0;
}

char * villas::node::redis_print(NodeCompat *n)
{
	auto *r = n->getData<struct redis>();

	std::stringstream ss;

	ss << "mode=" << r->mode
	   << ", key=" << r->key
	   << ", notify=" << (r->notify ? "yes" : "no");

	if (!r->notify)
		ss << ", rate=" << r->rate;

	ss << ", " << r->options;

	return strdup(ss.str().c_str());
}

int villas::node::redis_prepare(NodeCompat *n)
{
	int ret;
	auto *r = n->getData<struct redis>();

	r->options.type = r->options.path.empty()
		? sw::redis::ConnectionType::TCP
		: sw::redis::ConnectionType::UNIX;

	if (r->key.empty())
		r->key = n->getNameShort();

	ret = queue_signalled_init(&r->queue, 1024);
	if (ret)
		return ret;

	ret = pool_init(&r->pool, 1024, SAMPLE_LENGTH(64));
	if (ret)
		return ret;

	r->conn = RedisConnection::get(r->options);
	if (!r->conn)
		return -1;

	return 0;
}

int villas::node::redis_start(NodeCompat *n)
{
	auto *r = n->getData<struct redis>();

	r->formatter->start(n->getInputSignals(false), ~(int) SampleFlags::HAS_OFFSET);

	if (!r->notify)
		r->task.setRate(r->rate);

	switch (r->mode) {
		case RedisMode::CHANNEL:
			r->conn->subscribe(n, r->key);
			break;

		case RedisMode::HASH:
		case RedisMode::KEY:
			if (r->notify) {
				auto pattern = fmt::format("__keyspace@{}__:{}", r->options.db, r->key);
				r->conn->subscribe(n, pattern);
			}
			break;
	}

	r->conn->start();

	return 0;
}

int villas::node::redis_stop(NodeCompat *n)
{
	auto *r = n->getData<struct redis>();

	r->conn->stop();

	if (!r->notify)
		r->task.stop();

	switch (r->mode) {
		case RedisMode::CHANNEL:
			r->conn->unsubscribe(n, r->key);
			break;

		case RedisMode::HASH:
		case RedisMode::KEY:
			if (r->notify) {
				auto pattern = fmt::format("__keyspace@{}__:{}", r->options.db, r->key);
				r->conn->unsubscribe(n, pattern);
			}
			break;
	}

	ret = queue_signalled_close(&r->queue);
	if (ret)
		return ret;

	return 0;
}

int villas::node::redis_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	auto *r = n->getData<struct redis>();

	/* Wait for new data */
	if (r->notify || r->mode == RedisMode::CHANNEL) {
		int pulled_cnt;
		struct Sample *pulled_smps[cnt];

		pulled_cnt = queue_signalled_pull_many(&r->queue, (void **) pulled_smps, cnt);

		sample_copy_many(smps, pulled_smps, pulled_cnt);
		sample_decref_many(pulled_smps, pulled_cnt);

		return pulled_cnt;
	}
	else {
		if (r->task.wait() == 0)
			throw SystemError("Failed to wait for task");

		return redis_get(n, smps, cnt);
	}
}

int villas::node::redis_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int ret;
	auto *r = n->getData<struct redis>();

	switch (r->mode) {
		case RedisMode::CHANNEL:
			for (unsigned i = 0; i < cnt; i++) {
				char buf[1500];
				size_t wbytes;

				ret = r->formatter->sprint(buf, sizeof(buf), &wbytes, &smps[i], cnt);
				if (ret < 0)
					return ret;

				auto value = std::string_view(buf, wbytes);

				r->conn->context.publish(r->key, value);
			}
			break;

		case RedisMode::KEY: {
			char buf[1500];
			size_t wbytes;

			ret = r->formatter->sprint(buf, sizeof(buf), &wbytes, smps, cnt);
			if (ret < 0)
				return ret;

			auto value = std::string_view(buf, wbytes);

			r->conn->context.set(r->key, value);
			break;
		}

		case RedisMode::HASH: {
			/* We only update the signals with their latest value here. */
			struct Sample *smp = smps[cnt - 1];

			std::unordered_map<std::string, std::string> kvs;

			for (unsigned j = 0; j < MIN(smp->signals->size(), smp->length); j++) {
				const auto sig = smp->signals->getByIndex(j);
				const auto *data = &smp->data[j];

				kvs[sig->name] = data->toString(sig->type);
			}

			r->conn->context.hmset(r->key, kvs.begin(), kvs.end());
			break;
		}
	}

	return cnt;
}

int villas::node::redis_poll_fds(NodeCompat *n, int fds[])
{
	auto *r = n->getData<struct redis>();

	fds[0] = r->notify
		? queue_signalled_fd(&r->queue)
		: r->task.getFD();

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

	static NodeCompatFactory ncp(&p);
}
