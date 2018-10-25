/** Socket API endpoint.
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

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <exception>
#include <algorithm>
#include <filesystem>

#include <villas/config.h>
#include <villas/exceptions.hpp>
#include <villas/utils.h>
#include <villas/super_node.hpp>
#include <villas/api/server.hpp>
#include <villas/api/sessions/socket.hpp>

using namespace villas::node::api;

Server::Server(Api *a) :
	state(STATE_INITIALIZED),
	api(a)
{

}

Server::~Server()
{
	assert(state != STATE_STARTED);
}

void Server::start()
{
	int ret;

	assert(state != STATE_STARTED);

	sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sd < 0)
		throw new SystemError("Failed to create Api socket");

	pollfd pfd = {
		.fd = sd,
		.events = POLLIN
	};

	pfds.push_back(pfd);
	sessions.push_back(nullptr);

	struct sockaddr_un sun = { .sun_family = AF_UNIX };

#ifdef __GNU__
	std::filesystem::path socketPath = PREFIX "/var/lib/villas";
	if (!std::filesystem::exists(socketPath.parent_path())) {
		logging.get("api")->info("Creating directory for API socket: {}", socketPath);
		std::filesystem::create_directories(socketPath);
	}

	socketPath += "/node-" + api->getSuperNode()->getName() + ".sock";

	if (std::filesystem::exists(socketPath)) {
		logging.get("api")->info("Removing existing socket: {}", socketPath);
		std::filesystem::remove(socketPath);
	}

	strncpy(sun.sun_path, socketPath.c_str(), sizeof(sun.sun_path) - 1);
#else
	std::string socketPath = fmt::format(PREFIX "/var/lib/villas/node-{}.sock", api->getSuperNode()->getName());

	ret = unlink(socketPath.c_str());
	if (ret && errno != ENOENT)
		throw new SystemError("Failed to unlink API socket");

	strncpy(sun.sun_path, socketPath.c_str(), sizeof(sun.sun_path) - 1);
#endif

	ret = bind(sd, (struct sockaddr *) &sun, sizeof(struct sockaddr_un));
	if (ret)
		throw new SystemError("Failed to bind API socket");

	ret = listen(sd, 5);
	if (ret)
		throw new SystemError("Failed to listen on API socket");

	state = STATE_STARTED;
}

void Server::stop()
{
	int ret;

	assert(state == STATE_STARTED);

	ret = close(sd);
	if (ret)
		throw new SystemError("Failed to close API socket");;

	state = STATE_STOPPED;
}

void Server::run(int timeout)
{
	int ret;

	assert(state == STATE_STARTED);

	ret = poll(pfds.data(), pfds.size(), timeout);
	if (ret < 0)
		throw new SystemError("Failed to poll on API socket");;

	for (unsigned i = 0; i < pfds.size(); i++) {
		auto &pfd = pfds[i];
		auto s = sessions[i];

		if (pfd.revents & POLLOUT) {
			if (s)
				s->write();
		}

		if (pfd.revents & POLLIN) {
			/* New connection */
			if (s) {
				ret = s->read();
				if (ret < 0)
					closeSession(s);
			}
			else
				acceptNewSession();
		}
	}
}

void Server::acceptNewSession() {
	int fd = ::accept(sd, nullptr, nullptr);

	auto s = new sessions::Socket(api, fd);

	pollfd pfd = {
		.fd = fd,
		.events = POLLIN | POLLOUT
	};

	pfds.push_back(pfd);
	sessions.push_back(s);

	api->sessions.push_back(s);
}

void Server::closeSession(sessions::Socket *s)
{
	api->sessions.remove(s);

	ptrdiff_t pos = std::find(sessions.begin(), sessions.end(), s) - sessions.begin();

	if (pos < (ptrdiff_t) sessions.size()) {
		pfds.erase(pfds.begin() + pos);
		sessions.erase(sessions.begin() + pos);

		delete s;
	}
}
