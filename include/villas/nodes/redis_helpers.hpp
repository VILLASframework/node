/** Redis node-type helpers
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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

#pragma once

#include <chrono>
#include <functional>
#include <sw/redis++/redis++.h>
#include <spdlog/fmt/ostr.h>

#include <villas/node/config.hpp>

namespace std {

template<typename _rep, typename ratio>
struct hash<std::chrono::duration<_rep, ratio>>
{
	std::size_t operator()(std::chrono::duration<_rep, ratio> const& s) const
	{
		return std::hash<_rep>{}(s.count());
	}
};

template <>
struct hash<sw::redis::tls::TlsOptions>
{
	std::size_t operator()(const sw::redis::tls::TlsOptions& t) const
	{
#ifdef REDISPP_WITH_TLS
		return hash<bool>()(t.enabled) ^
		       hash<std::string>()(t.cacert) ^
		       hash<std::string>()(t.cacertdir) ^
		       hash<std::string>()(t.cert) ^
		       hash<std::string>()(t.key) ^
		       hash<std::string>()(t.sni);
#else
		return 0;
#endif /* REDISPP_WITH_TLS */
	}
};

template <>
struct hash<sw::redis::ConnectionOptions>
{
	std::size_t operator()(const sw::redis::ConnectionOptions& o) const
	{
		return hash<int>()(static_cast<int>(o.type)) ^
		       hash<std::string>()(o.host) ^
		       hash<int>()(o.port) ^
		       hash<std::string>()(o.path) ^
		       hash<std::string>()(o.user) ^
		       hash<std::string>()(o.password) ^
		       hash<int>()(o.db) ^
		       hash<bool>()(o.keep_alive) ^
		       hash<std::chrono::milliseconds>()(o.connect_timeout) ^
		       hash<std::chrono::milliseconds>()(o.socket_timeout) ^
		       hash<sw::redis::tls::TlsOptions>()(o.tls);
	}
};

} /* namespace std */

namespace sw {
namespace redis {

bool operator==(const tls::TlsOptions &o1, const tls::TlsOptions &o2)
{
#ifdef REDISPP_WITH_TLS
	return o1.enabled == o2.enabled &&
	       o1.cacert == o2.cacert &&
	       o1.cacertdir == o2.cacertdir &&
	       o1.cert == o2.cert &&
	       o1.key == o2.key &&
	       o1.sni == o2.sni;
#else
	return true;
#endif /* REDISPP_WITH_TLS */
}

bool operator==(const ConnectionOptions &o1, const ConnectionOptions &o2)
{
	return o1.type == o2.type &&
	       o1.host == o2.host &&
	       o1.port == o2.port &&
	       o1.path == o2.path &&
	       o1.user == o2.user &&
	       o1.password == o2.password &&
	       o1.db == o2.db &&
	       o1.keep_alive == o2.keep_alive &&
	       o1.connect_timeout == o2.connect_timeout &&
	       o1.socket_timeout == o2.socket_timeout &&
	       o1.tls == o2.tls;
}

#ifdef REDISPP_WITH_TLS
template<typename OStream>
OStream &operator<<(OStream &os, const tls::TlsOptions &t)
{
	os << "tls.enabled=" << (t.enabled ? "yes" : "no");

	if (t.enabled) {
		if (!t.cacert.empty())
			os << ", tls.cacert=" << t.cacert;

		if (!t.cacertdir.empty())
			os << ", tls.cacertdir=" << t.cacertdir;

		if (!t.cert.empty())
			os << ", tls.cert=" << t.cert;

		if (!t.key.empty())
			os << ", tls.key=" << t.key;

		if (!t.sni.empty())
			os << ", tls.sni=" << t.sni;
	}

	return os;
}
#endif /* REDISPP_WITH_TLS */

template<typename OStream>
OStream &operator<<(OStream &os, const ConnectionType &t)
{
	switch (t) {
		case ConnectionType::TCP:
			os << "tcp";
			break;

		case ConnectionType::UNIX:
			os << "unix";
			break;
	}

	return os;
}

template<typename OStream>
OStream &operator<<(OStream &os, const ConnectionOptions &o)
{
	os << "type=" << o.type;

	switch (o.type) {
		case ConnectionType::TCP:
			os << ", host=" << o.host << ", port=" << o.port;
			break;

		case ConnectionType::UNIX:
			os << ", path=" << o.path;
			break;
	}

	os << ", db=" << o.db
	   << ", user=" << o.user
	   << ", keepalive=" << (o.keep_alive ? "yes" : "no");

#ifdef REDISPP_WITH_TLS
	os << ", " << o.tls;
#endif

	return os;
}

} /* namespace redis */
} /* namespace sw */

template<typename OStream>
OStream &operator<<(OStream &os, const enum villas::node::RedisMode &m)
{
	switch (m) {
		case villas::node::RedisMode::KEY:
			os << "key";
			break;

		case villas::node::RedisMode::HASH:
			os << "hash";
			break;

		case villas::node::RedisMode::CHANNEL:
			os << "channel";
			break;
	}

	return os;
}
