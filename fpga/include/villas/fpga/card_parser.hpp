#pragma once

#include <jansson.h>
#include <spdlog/logger.h>
#include <villas/exceptions.hpp>
#include <villas/log.hpp>

class CardParser {
public:
  std::shared_ptr<spdlog::logger> logger;

  json_t *json_ips = nullptr;
  json_t *json_paths = nullptr;
  const char *pci_slot = nullptr;
  const char *pci_id = nullptr;
  int do_reset = 0;
  int affinity = 0;
  int polling = 0;

  CardParser(json_t *json_card) : logger(villas::Log::get("CardParser")) {
    json_error_t err;
    int ret = json_unpack_ex(
        json_card, &err, 0, "{ s: o, s?: i, s?: b, s?: s, s?: s, s?: b, s?: o}",
        "ips", &json_ips, "affinity", &affinity, "do_reset", &do_reset, "slot",
        &pci_slot, "id", &pci_id, "polling", &polling, "paths", &json_paths);

    if (ret != 0)
      throw villas::ConfigError(json_card, err, "", "Failed to parse card");
  }
};
