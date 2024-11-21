/* Node type: C37-118.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2024 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cerrno>
#include <cstdint>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <villas/exceptions.hpp>
#include <villas/nodes/c37_118.hpp>
#include <villas/nodes/c37_118/parser.hpp>
#include <villas/socket_addr.hpp>
#include <villas/utils.hpp>

using namespace std::literals::string_view_literals;
using namespace villas::node;
using namespace villas::node::c37_118;
using namespace villas::node::c37_118::types;

void C37_118::Client::send(Frame::Variant message, timespec ts) {
  std::vector send_buf = parser.serialize({
    .idcode = idcode,
    .soc = static_cast<uint32_t>(ts.tv_sec & 0xFFFFFF),
    .fracsec = static_cast<uint32_t>(ts.tv_nsec * static_cast<int64_t>(config->time_base)) / 1'000'000'000,
    .message = message,
  }, config ? &*config : nullptr);

  size_t sum = 0;
  int ret = 0;
  if (socktype == SOCK_DGRAM) {
    do {
      ret = ::send(connection_fd, send_buf.data() + sum, send_buf.size() - sum, 0);
      sum += ret;
    } while (sum < send_buf.size() && ret > 0);
  } else {
    ret = ::write(connection_fd, send_buf.data(), send_buf.size());
  }

  if (ret == 0)
    throw RuntimeError{"end of stream"};

  if (ret < 0)
    throw SystemError{"send error {}", ::strerror(errno)};

  sum += ret;
}

std::optional<Frame> C37_118::Client::recv() {
  int ret;

  if (socktype == SOCK_DGRAM)
    ret = ::recv(connection_fd, recv_buf.data(), recv_buf.size(), 0);
  else
    ret = ::read(connection_fd, recv_buf.data(), recv_buf.size());

  if (ret == 0)
    throw RuntimeError{"end of stream"};

  if (ret == -EWOULDBLOCK)
    return std::nullopt;

  if (ret < 0)
    throw SystemError{"socket error {}", ::strerror(errno)};

  auto frame = parser.deserialize(recv_buf.data(), ret, config ? &*config : nullptr);
  if (!frame.has_value())
    return std::nullopt;

  std::copy(recv_buf.begin() + frame->framesize, recv_buf.end(), recv_buf.begin());
  return *frame;
}

int C37_118::_read(struct Sample *smps[], unsigned cnt) {
  auto smp = smps[0];

  if (auto *client = std::get_if<Client>(&role)) {
    auto frame = client->recv();
    if (!frame.has_value())
      return 0;

    auto data = std::get_if<Data>(&frame->message);
    if (data == nullptr)
      return 0;

    std::size_t s = 0;
    for (auto &pmu : data->pmus) {
      for (auto p = pmu.phasor.begin(); p != pmu.phasor.end() && s < smp->capacity; ++p)
        smp->data[s++].z = *p;

      if (s < smp->capacity)
        smp->data[s++].f = pmu.freq;

      if (s < smp->capacity)
        smp->data[s++].f = pmu.dfreq;

      for (auto a = pmu.analog.begin(); a != pmu.analog.end() && s < smp->capacity; ++a)
        smp->data[s++].f = *a;

      for (auto d = pmu.digital.begin(); d != pmu.digital.end() && s < smp->capacity; ++d)
        for (auto i = 0; i < 16; i++)
          smp->data[s++].b = (*d >> i) & 1;
    }

    smp->length = s;
    smp->ts.origin = {
      .tv_sec = frame->soc,
      .tv_nsec = static_cast<int64_t>(frame->fracsec & 0xFFFFFF) * 1'000'000'000 / client->config->time_base,
    };
    smp->flags = (int) SampleFlags::HAS_DATA | (int) SampleFlags::HAS_TS_ORIGIN;

    return 1;
  }

  return 0;
}

C37_118::~C37_118() {

}

void C37_118::parseServer(json_t *json) {
  json_error_t err;

  char *addr = nullptr;
  char *transport = nullptr;
  int port = 0;
  int idcode = 0;
  json_t *config_json = nullptr;
  if (json_unpack_ex(json, &err, 0, "{ s:s, s:s, s:i, s:i, s:o }",
                     "transport", &transport, "address", &addr, "port", &port, "idcode", &idcode,
                     "config", &config_json))
    throw ConfigError(json, err, "node-config-node-c37.118-server");

  int socktype;
  if (transport == "tcp"sv)
    socktype = SOCK_STREAM;
  else if (transport == "udp"sv)
    socktype = SOCK_DGRAM;
  else
    throw RuntimeError{"invalid transport {}", transport};

  throw RuntimeError{"unimplemented"};

  this->role = Server {
    .addr = std::string(addr),
    .port = static_cast<uint16_t>(port),
    .socktype = socktype,
  };
}

void C37_118::parseClient(json_t *json) {
  json_error_t err;
  char *addr = nullptr;
  char *transport = nullptr;
  int port = 0;
  int idcode = 0;
  if (json_unpack_ex(json, &err, 0, "{ s:s, s:s, s:i, s:i }",
                     "transport", &transport, "address", &addr, "port", &port, "idcode", &idcode))
    throw ConfigError(json, err, "node-config-node-c37.118-client");

  int socktype;
  if (transport == "tcp"sv)
    socktype = SOCK_STREAM;
  else if (transport == "udp"sv)
    socktype = SOCK_DGRAM;
  else
    throw RuntimeError{"invalid transport {}", transport};

  this->role = Client {
    .addr = std::string(addr),
    .port = static_cast<uint16_t>(port),
    .idcode = static_cast<uint16_t>(idcode),
    .socktype = socktype,
  };
}

int C37_118::parse(json_t *json) {
  json_error_t err;
  json_t *server = nullptr;
  json_t *client = nullptr;
  if (json_unpack_ex(json, &err, 0, "{ s?:o, s?:o }",
                     "server", &server, "client", &client))
    throw ConfigError(json, err, "node-config-node-c37.118");

  if (server && client) throw RuntimeError{"c37_118: can't be both server and client"};
  else if (server) parseServer(server);
  else if (client) parseClient(client);

  return Node::parse(json);
}

void C37_118::prepareServer(Server &server) {
  addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = server.socktype;
  hints.ai_flags = AI_PASSIVE;

  addrinfo *addrs_ret;
  std::string service = std::to_string(server.port);
  if (auto err = ::getaddrinfo(server.addr.c_str(), service.c_str(), &hints,
                               &addrs_ret)) // TODO: make configurable
    throw SystemError{"c37_118: getaddrinfo {}", ::gai_strerror(err)};

  auto addrs = std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>{
      addrs_ret, &freeaddrinfo};
  int sock;
  addrinfo *addr;
  for (addr = addrs.get(); addr != nullptr; addr = addr->ai_next) {
    sock = ::socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

    if (sock < 0)
      continue;

    if (::bind(sock, addr->ai_addr, addr->ai_addrlen) == 0) {
      server.listener_fd = sock;
      break;
    }

    ::close(sock);
  }

  if (addr == nullptr)
    throw RuntimeError{"could not bind port {} on {}", server.port, server.addr};

  throw RuntimeError{"unimplemented"};
}

void C37_118::prepareClient(Client &client) {
  addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = client.socktype;

  addrinfo *addrs_ret;
  std::string service = std::to_string(client.port);
  if (auto err = ::getaddrinfo(client.addr.c_str(), service.c_str(), &hints,
                               &addrs_ret))
    throw SystemError{"c37_118: getaddrinfo {}", ::gai_strerror(err)};

  auto addrs = std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>{
      addrs_ret, &freeaddrinfo};
  addrinfo *addr;
  for (addr = addrs.get(); addr != nullptr; addr = addr->ai_next) {
    int sock = ::socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

    if (sock < 0)
      continue;

    if (::connect(sock, addr->ai_addr, addr->ai_addrlen) == 0) {
      client.connection_fd = sock;
      break;
    }

    ::close(sock);
  }

  if (addr == nullptr)
    throw RuntimeError{"could not connect to port {} on {}", client.port, client.addr};

  client.send(Command{Command::GET_CONFIG2});
  for (;;) {
    auto frame = client.recv();
    if (!frame.has_value())
      continue;

    auto config2 = std::get_if<Config2>(&frame->message);
    if (config2 == nullptr)
      continue;

    client.config = *config2;
    auto &sigs = in.signals;
    sigs->clear();
    for (auto &pmu : client.config->pmus) {
      for (auto &p : pmu.phinfo)
        sigs->emplace_back(std::make_shared<Signal>(p.chnam, p.unit_str(), SignalType::COMPLEX));

      sigs->emplace_back(std::make_shared<Signal>("FREQ", "Hz", SignalType::FLOAT));
      sigs->emplace_back(std::make_shared<Signal>("DFREQ", "Hz/s", SignalType::FLOAT));

      for (auto &a : pmu.aninfo)
        sigs->emplace_back(std::make_shared<Signal>(a.chnam, a.unit_str(), SignalType::FLOAT));

      for (auto &d : pmu.dginfo)
        for (size_t i = 0; i < 16; ++i)
          sigs->emplace_back(std::make_shared<Signal>(d.chnam[i], "", SignalType::BOOLEAN));
    }

    break;
  }
}

int C37_118::prepare() {
  std::visit(villas::utils::overloaded {
    [&](Server &server){ prepareServer(server); },
    [&](Client &client){ prepareClient(client); },
  }, role);

  return Node::prepare();
}

int C37_118::start() {
  int ret = Node::start();
  if (ret)
    return ret;

  std::visit(villas::utils::overloaded {
    [&](Server &server){  },
    [&](Client &client){
      client.send(Command{Command::DATA_START});
    },
  }, role);

  return 0;
}

int C37_118::stop() {
  int ret = Node::stop();
  if (ret)
    return ret;

  std::visit(villas::utils::overloaded {
    [&](Server &server){  },
    [&](Client &client){
      client.send(Command{Command::DATA_STOP});
    },
  }, role);

  return 0;
}

std::vector<int> C37_118::getPollFDs() {
  return std::visit(villas::utils::overloaded {
    [&](Server &server){
      return std::vector<int> { };
    },
    [&](Client &client){
      return std::vector<int> { client.connection_fd };
    },
  }, role);
}

// Register node
static char n[] = "c37.118";
static char d[] = "A node for a C37.118 TCP/UDP server/client";
static NodePlugin<C37_118, n, d,
                  (int)NodeFactory::Flags::SUPPORTS_READ |
                      (int)NodeFactory::Flags::SUPPORTS_WRITE |
                      (int)NodeFactory::Flags::SUPPORTS_POLL>
    p;
