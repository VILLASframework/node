/** REST-API-releated functions.
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

#include <villas/api.hpp>
#include <villas/web.hpp>
#include <villas/api/session.hpp>
#include <villas/api/request.hpp>
#include <villas/utils.hpp>
#include <villas/node/config.h>
#include <villas/memory.h>
#include <villas/compat.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::node::api;

InvalidMethod::InvalidMethod(Request *req) :
	BadRequest("The '{}' API endpoint does not support {} requests",
			req->factory->getName(),
			Session::methodToString(req->method)
	)
{ }

Api::Api(SuperNode *sn) :
	logger(logging.get("api")),
	state(State::INITIALIZED),
	super_node(sn)
{ }

Api::~Api()
{
	assert(state != State::STARTED);
}

void Api::start()
{
	assert(state != State::STARTED);

	logger->info("Starting sub-system");

	running = true;
	thread = std::thread(&Api::worker, this);

	state = State::STARTED;
}

void Api::stop()
{
	assert(state == State::STARTED);

	logger->info("Stopping sub-system");

	for (Session *s : sessions)
		s->shutdown();

	for (int i = 0; i < 2 && sessions.size() > 0; i++) {
		logger->info("Waiting for {} sessions to terminate", sessions.size());
		usleep(1 * 1e6);
	}

	running = false;
	pending.push(nullptr); /* unblock thread */
	thread.join();

	state = State::STOPPED;
}

void Api::worker()
{
	logger->info("Started worker");

	/* Process pending requests */
	while (!pending.empty() && running) {
		Session *s = pending.pop();
		if (s) {
			/* Check that the session is still alive */
			auto it = std::find(sessions.begin(), sessions.end(), s);
			if (it != sessions.end())
				s->execute();
		}
	}

	logger->info("Stopped worker");
}
