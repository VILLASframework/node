#pragma once

#include <pthread.h>

#include <villas/node.hpp>
#include <villas/format.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class GatewayNode : public Node {
protected:
  int parse(json_t *json) override;

  int _read(struct Sample *smps[], unsigned cnt) override;
  int _write(struct Sample *smps[], unsigned cnt) override;

public:
  GatewayNode(const uuid_t &id = {}, const std::string &name = "");
  enum ApiType { gRPC };

  struct Direction {
    Sample *sample;
    pthread_cond_t cv;
    pthread_mutex_t mutex;
    char* buf;
    size_t buflen;
    size_t wbytes;
  };


  Direction read, write;
  std::string address;
  ApiType type;

  Format *formatter;

  char *buf;
  size_t buflen;
  size_t wbytes;

  int prepare() override;

  int check() override;

  int start() override;

  int stop() override;

  ~GatewayNode();
};

} // namespace node
} // namespace villas
