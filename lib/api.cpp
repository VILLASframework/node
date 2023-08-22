/** REST-API-releated functions.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/api.hpp>
#include <villas/web.hpp>
#include <villas/api/session.hpp>
#include <villas/api/request.hpp>
#include <villas/utils.hpp>
#include <villas/node/config.hpp>
#include <villas/node/memory.hpp>
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
	if (state == State::STARTED)
		stop();
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
	while (running) {
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
