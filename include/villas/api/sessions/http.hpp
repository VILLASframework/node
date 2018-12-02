/** HTTP Api session.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/api/sessions/wsi.hpp>

extern "C" int api_http_protocol_cb(lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

namespace villas {
namespace node {

/* Forward declarations */
class SuperNode;
class Api;

namespace api {
namespace sessions {

class Http : public Wsi {

public:
	Http(Api *s, lws *w);
	virtual ~Http() { };

	void read(void *in, size_t len);

	int complete();

	int write();

	virtual std::string getName();
};

} // sessions
} // api
} // node
} // villas
