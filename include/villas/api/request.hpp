/** API Request.
 *
 * @file
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

#include <vector>
#include <regex>
#include <jansson.h>

#include <villas/log.hpp>
#include <villas/buffer.hpp>
#include <villas/plugin.hpp>
#include <villas/super_node.hpp>
#include <villas/api/session.hpp>

#define RE_UUID "[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}"

namespace villas {
namespace node {
namespace api {

/* Forward declarations */
class Session;
class Response;
class RequestFactory;

class Request {

	friend Session;
	friend RequestFactory;
	friend Response;

protected:
	Session *session;

	Logger logger;
	Buffer buffer;

public:
	std::vector<std::string> matches;
	Session::Method method;
	unsigned long contentLength;
	json_t *body;

	RequestFactory *factory;

	Request(Session *s) :
		session(s),
		method(Session::Method::UNKNOWN),
		contentLength(0),
		body(nullptr),
		factory(nullptr)
	{ }

	virtual
	~Request()
	{
		if (body)
			json_decref(body);
	}

	virtual
	void prepare()
	{ }

	virtual
	Response * execute() = 0;

	virtual
	void decode();

	const std::string & getMatch(int idx) const
	{
		return matches[idx];
	}

	std::string getQueryArg(const std::string &arg)
	{
		char buf[1024];
		const char *val;

		val = lws_get_urlarg_by_name(session->wsi, (arg + "=").c_str(), buf, sizeof(buf));

		return val ? std::string(val) : std::string();
	}

	std::string getHeader(enum lws_token_indexes hdr)
	{
		char buf[1024];

		lws_hdr_copy(session->wsi, buf, sizeof(buf), hdr);

		return std::string(buf);
	}

	virtual
	std::string toString();
};

class RequestFactory : public plugin::Plugin {

public:
	using plugin::Plugin::Plugin;

	virtual
	bool match(const std::string &uri, std::smatch &m) const = 0;

	virtual
	Request * make(Session *s) = 0;

	static
	Request * create(Session *s, const std::string &uri, Session::Method meth, unsigned long ct);

	virtual
	void init(Request *r)
	{
		r->logger = getLogger();
	}

	virtual
	std::string
	getType() const
	{
		return "api:request";
	}
};

template<typename T, const char *name, const char *re, const char *desc>
class RequestPlugin : public RequestFactory {

protected:
	std::regex regex;

public:
	RequestPlugin() :
		RequestFactory(),
		regex(re)
	{ }

	virtual Request *
	make(Session *s)
	{
		auto *r = new T(s);

		init(r);

		return r;
	}

	// Get plugin name
	virtual std::string
	getName() const
	{
		return name;
	}

	// Get plugin description
	virtual std::string
	getDescription() const
	{
		return desc;
	}

	virtual bool
	match(const std::string &uri, std::smatch &match) const
	{
		return std::regex_match(uri, match, regex);
	}
};

} /* namespace api */
} /* namespace node */
} /* namespace villas */
