#pragma once

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <jansson.h>
#include <spdlog/logger.h>

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
  json_t *devices = nullptr;
  std::vector<std::string> device_names;

  CardParser(json_t *json_card) : logger(villas::logging.get("CardParser")) {
    json_error_t err;
    int ret = json_unpack_ex(
        json_card, &err, 0,
        "{ s: o, s?: i, s?: b, s?: s, s?: s, s?: b, s?: o, s?: o }", "ips",
        &json_ips, "affinity", &affinity, "do_reset", &do_reset, "slot",
        &pci_slot, "id", &pci_id, "polling", &polling, "paths", &json_paths,
        "devices", &devices);

    if (ret != 0)
      throw villas::ConfigError(json_card, err, "", "Failed to parse card");

    // Devices array parsing
    size_t index;
    json_t *value;
    json_array_foreach(devices, index, value) {
      auto str = json_string_value(value);
      this->device_names.push_back(str);
    }
  }
};
