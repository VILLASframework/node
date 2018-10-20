/** WebSockets API session.
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

extern "C" int api_ws_protocol_cb(lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

namespace villas {
namespace node {

/* Forward declarations */
class Api;

namespace api {
namespace sessions {

class WebSocket : public Wsi {

public:
	WebSocket(Api *a, lws *w);

	virtual ~WebSocket();

	virtual std::string getName();

	int read(void *in, size_t len);

	int write();
};

} // sessions
} // api
} // node
} // villas
