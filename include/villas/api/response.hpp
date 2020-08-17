/** API response.
 *
 * @file
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

#pragma once

#include <jansson.h>

#include <villas/log.hpp>
#include <villas/exceptions.hpp>
#include <villas/plugin.hpp>
#include <villas/api.hpp>

namespace villas {
namespace node {
namespace api {

/* Forward declarations */
class Session;
class Request;

class Response {

protected:
	Session *session;
	Logger logger;

public:
	json_t *response;
	int code;

	Response(Session *s, json_t *resp = nullptr);

	virtual ~Response();

	int
	getCode() const
	{
		return code;
	}

	/** Return JSON representation of response as used by API sockets. */
	virtual json_t *
	toJson()
	{
		return response;
	}
};

class ErrorResponse : public Response {

protected:
	std::string error;

public:
	ErrorResponse(Session *s, const RuntimeError &e) :
		Response(s),
		error(e.what())
	{
		code = 500;
	}

	ErrorResponse(Session *s, const Error &e) :
		Response(s),
		error(e.what())
	{
		code = e.code;
	}

	virtual json_t * toJson();
};

} /* namespace api */
} /* namespace node */
} /* namespace villas */
