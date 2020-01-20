/** Unix domain socket Api session.
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

#include <villas/compat.h>
#include <villas/log.hpp>
#include <villas/api/sessions/socket.hpp>

using namespace villas::node::api::sessions;

Socket::Socket(Api *a, int s) :
	Session(a),
	sd(s)
{

}

Socket::~Socket()
{
	close(sd);
}

std::string Socket::getName()
{
	std::stringstream ss;

	ss << Session::getName() << ", mode=socket";

	return ss.str();
}

int Socket::read()
{
	json_t *j;
	json_error_t err;

	j = json_loadfd(sd, JSON_DISABLE_EOF_CHECK, &err);
	if (!j)
		return -1;

	request.queue.push(j);

	api->pending.push(this);

	return 0;
}

int Socket::write()
{
	int ret;
	json_t *j;

	while (!response.queue.empty()) {
		j = response.queue.pop();

		ret = json_dumpfd(j, sd, 0);
		if (ret)
			return ret;

		char nl = '\n';
		send(sd, &nl, 1, 0);
	}

	return 0;
}
