/** LWS Api session.
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

#include <sstream>

#include <villas/api/sessions/wsi.hpp>

using namespace villas::node::api::sessions;

void Wsi::runPendingActions()
{
	Session::runPendingActions();

	logger->debug("Ran pending actions. Triggering on_writeable callback: wsi={}", (void *) wsi);

	web->callbackOnWritable(wsi);
}

void Wsi::shutdown()
{
	state = State::SHUTDOWN;

	web->callbackOnWritable(wsi);
}

std::string Wsi::getName()
{
	std::stringstream ss;

	ss << Session::getName();

	if (wsi) {
		char name[128];
		char ip[128];

		lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi), name, sizeof(name), ip, sizeof(ip));

		ss << ", remote.name=" << name << ", remote.ip=" << ip;
	}

	return ss.str();

}

Wsi::Wsi(Api *a, lws *w) :
	Session(a),
	wsi(w)
{
	state = Session::State::ESTABLISHED;

	lws_context *user_ctx = lws_get_context(wsi);
	void *ctx = lws_context_user(user_ctx);

	web = static_cast<Web*>(ctx);
}
