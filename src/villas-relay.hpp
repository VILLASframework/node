/* Simple WebSocket relay facilitating client-to-client connections.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <map>
#include <memory>
#include <queue>
#include <vector>

#include <libwebsockets.h>
#include <uuid/uuid.h>

#include <villas/log.hpp>
#include <villas/tool.hpp>

namespace villas {
namespace node {
namespace tools {

// Forward declarations
class Relay;
class RelaySession;
class RelayConnection;

class InvalidUrlException {};

typedef std::string Identifier;

class Frame : public std::vector<uint8_t> {
public:
  Frame() {
    reserve(LWS_PRE);
    // lws_write() requires LWS_PRE bytes in front of the payload
    insert(end(), LWS_PRE, 0);
  }

  uint8_t *data() { return std::vector<uint8_t>::data() + LWS_PRE; }

  size_type size() { return std::vector<uint8_t>::size() - LWS_PRE; }
};

class RelaySession {

  friend RelayConnection;
  friend Relay;

protected:
  time_t created;
  uuid_t uuid;

  Identifier identifier;
  Logger logger;

  std::map<lws *, RelayConnection *> connections;

  int connects;

  static std::map<std::string, RelaySession *> sessions;

public:
  static RelaySession *get(Relay *r, lws *wsi);

  RelaySession(Relay *r, Identifier sid);

  ~RelaySession();

  json_t *toJson() const;
};

class RelayConnection {

protected:
  lws *wsi;

  std::shared_ptr<Frame> currentFrame;

  std::queue<std::shared_ptr<Frame>> outgoingFrames;

  RelaySession *session;

  char name[128];
  char ip[128];

  size_t created;
  size_t bytes_recv;
  size_t bytes_sent;

  size_t frames_recv;
  size_t frames_sent;

  bool loopback;

public:
  RelayConnection(Relay *r, lws *w, bool lo);
  ~RelayConnection();

  json_t *toJson() const;

  void write();
  void read(void *in, size_t len);
};

class Relay : public Tool {

public:
  friend RelaySession;

  Relay(int argc, char *argv[]);

protected:
  std::atomic<bool> stop;

  // The libwebsockets server context.
  lws_context *context;

  // The libwebsockets vhost.
  lws_vhost *vhost;

  bool loopback;
  int port;
  std::string protocol;
  std::string hostname;

  uuid_t uuid;

  // List of libwebsockets protocols.
  std::vector<lws_protocols> protocols;

  // List of libwebsockets extensions.
  static const std::vector<lws_extension> extensions;

  static const lws_http_mount mount;

  static void loggerCallback(int level, const char *msg);

  static int httpProtocolCallback(lws *wsi, enum lws_callback_reasons reason,
                                  void *user, void *in, size_t len);

  static int protocolCallback(lws *wsi, enum lws_callback_reasons reason,
                              void *user, void *in, size_t len);

  void usage();

  void parse();

  int main();

  void handler(int signal, siginfo_t *sinfo, void *ctx) { stop = true; }
};

} // namespace tools
} // namespace node
} // namespace villas
