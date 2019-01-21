/** Simple WebSocket relay facilitating client-to-client connections.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <vector>
#include <memory>

#include <spdlog/spdlog.h>
#include <uuid/uuid.h>

#include <libwebsockets.h>

/* Forward declarations */
lws_callback_function protocol_cb, http_protocol_cb;
class Session;
class Connection;

class InvalidUrlException { };

struct Options {
	bool loopback;
	int port;
	const char *protocol;
};

class Frame : public std::vector<uint8_t> {
public:
	Frame() {
		/* lws_write() requires LWS_PRE bytes in front of the payload */
		insert(end(), LWS_PRE, 0);
	}

	uint8_t * data() {
		return std::vector<uint8_t>::data() + LWS_PRE;
	}

	size_type size() {
		return std::vector<uint8_t>::size() - LWS_PRE;
	}
};

class Session {

protected:
	time_t created;
	uuid_t uuid;

public:
	typedef std::string Identifier;

	static Session * get(lws *wsi);

	Session(Identifier sid);

	~Session();

	json_t * toJson() const;

	Identifier identifier;

	std::map<lws *, Connection *> connections;

	int connects;
};

class Connection {

protected:
	lws *wsi;

	std::shared_ptr<Frame> currentFrame;

	std::queue<std::shared_ptr<Frame>> outgoingFrames;

	Session *session;

	char name[128];
	char ip[128];

	size_t created;
	size_t bytes_recv;
	size_t bytes_sent;

	size_t frames_recv;
	size_t frames_sent;

public:
	Connection(lws *w);

	~Connection();

	json_t * toJson() const;

	void write();
	void read(void *in, size_t len);
};
