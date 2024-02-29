/* Helper function for directly using VILLASfpga outside of VILLASnode
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2022-2023 Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2022-2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <jansson.h>
#include <regex>
#include <string>
#include <vector>

#include <CLI11.hpp>
#include <rang.hpp>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/utils.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/core.hpp>
#include <villas/fpga/ips/aurora_xilinx.hpp>
#include <villas/fpga/ips/dino.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/rtds.hpp>
#include <villas/fpga/utils.hpp>
#include <villas/fpga/vlnv.hpp>

using namespace villas;

static auto logger = villas::logging.get("streamer");

std::shared_ptr<std::vector<std::shared_ptr<fpga::ip::Node>>>
fpga::getAuroraChannels(std::shared_ptr<fpga::Card> card) {
  auto aurora_channels =
      std::make_shared<std::vector<std::shared_ptr<fpga::ip::Node>>>();
  for (int i = 0; i < 4; i++) {
    auto name = fmt::format("aurora_aurora_8b10b_ch{}", i);
    auto id = fpga::ip::IpIdentifier("xilinx.com:ip:aurora_8b10b:", name);
    auto aurora = std::dynamic_pointer_cast<fpga::ip::Node>(card->lookupIp(id));
    if (aurora == nullptr) {
      logger->error("No Aurora interface found on FPGA");
      throw std::runtime_error("No Aurora interface found on FPGA");
    }

    aurora_channels->push_back(aurora);
  }
  return aurora_channels;
}

fpga::ConnectString::ConnectString(std::string &connectString, int maxPortNum)
    : log(villas::logging.get("ConnectString")), maxPortNum(maxPortNum),
      bidirectional(false), invert(false), srcType(ConnectType::LOOPBACK),
      dstType(ConnectType::LOOPBACK), srcAsInt(-1), dstAsInt(-1) {
  parseString(connectString);
}

void fpga::ConnectString::parseString(std::string &connectString) {
  if (connectString.empty())
    return;

  if (connectString == "loopback") {
    logger->info("Connecting loopback");
    srcType = ConnectType::LOOPBACK;
    dstType = ConnectType::LOOPBACK;
    bidirectional = true;
    return;
  }
  static const std::regex regex("([0-9]+|stdin|stdout|pipe|dma|dino)([<\\->]+)("
                                "[0-9]+|stdin|stdout|pipe|dma|dino)");
  std::smatch match;

  if (!std::regex_match(connectString, match, regex) || match.size() != 4) {
    logger->error("Invalid connect string: {}", connectString);
    throw std::runtime_error("Invalid connect string");
  }

  if (match[2] == "<->") {
    bidirectional = true;
  } else if (match[2] == "<-") {
    invert = true;
  }

  std::string srcStr = (invert ? match[3] : match[1]);
  std::string dstStr = (invert ? match[1] : match[3]);

  srcAsInt = portStringToInt(srcStr);
  dstAsInt = portStringToInt(dstStr);
  if (srcAsInt == -1) {
    if (srcStr == "dino") {
      srcType = ConnectType::DINO;
    } else if (srcStr == "dma" || srcStr == "pipe" || srcStr == "stdin" ||
               srcStr == "stdout") {
      srcType = ConnectType::DMA;
    } else {
      throw std::runtime_error("Invalid source type");
    }
  } else {
    srcType = ConnectType::AURORA;
  }
  if (dstAsInt == -1) {
    if (dstStr == "dino") {
      dstType = ConnectType::DINO;
    } else if (dstStr == "dma" || dstStr == "pipe" || dstStr == "stdin" ||
               dstStr == "stdout") {
      dstType = ConnectType::DMA;
    } else {
      throw std::runtime_error("Invalid destination type");
    }
  } else {
    dstType = ConnectType::AURORA;
  }
}

int fpga::ConnectString::portStringToInt(std::string &str) const {
  if (str == "stdin" || str == "stdout" || str == "pipe" || str == "dma" ||
      str == "dino") {
    return -1;
  } else {
    const int port = std::stoi(str);

    if (port > maxPortNum || port < 0)
      throw std::runtime_error("Invalid port number");

    return port;
  }
}

// parses a string like "1->2" or "1<->stdout" and configures the crossbar accordingly
void fpga::ConnectString::configCrossBar(
    std::shared_ptr<villas::fpga::Card> card) const {

  auto dma = std::dynamic_pointer_cast<fpga::ip::Dma>(
      card->lookupIp(fpga::Vlnv("xilinx.com:ip:axi_dma:")));
  if (dma == nullptr) {
    logger->error("No DMA found on FPGA ");
    throw std::runtime_error("No DMA found on FPGA");
  }

  if (isDmaLoopback()) {
    log->info("Configuring DMA loopback");
    dma->connectLoopback();
    return;
  }

  auto aurora_channels = getAuroraChannels(card);

  auto dinoDac = std::dynamic_pointer_cast<fpga::ip::DinoDac>(
      card->lookupIp(fpga::Vlnv("xilinx.com:module_ref:dinoif_dac:")));
  if (dinoDac == nullptr) {
    logger->error("No Dino DAC found on FPGA ");
    throw std::runtime_error("No Dino DAC found on FPGA");
  }

  auto dinoAdc = std::dynamic_pointer_cast<fpga::ip::DinoAdc>(
      card->lookupIp(fpga::Vlnv("xilinx.com:module_ref:dinoif_fast:")));
  if (dinoAdc == nullptr) {
    logger->error("No Dino ADC found on FPGA ");
    throw std::runtime_error("No Dino ADC found on FPGA");
  }

  log->info("Connecting {} to {}, {}directional",
            (srcAsInt == -1 ? connectTypeToString(srcType)
                            : std::to_string(srcAsInt)),
            (dstAsInt == -1 ? connectTypeToString(dstType)
                            : std::to_string(dstAsInt)),
            (bidirectional ? "bi" : "uni"));

  std::shared_ptr<fpga::ip::Node> src;
  std::shared_ptr<fpga::ip::Node> dest;
  if (srcType == ConnectType::DINO) {
    src = dinoAdc;
  } else if (srcType == ConnectType::DMA) {
    src = dma;
  } else {
    src = (*aurora_channels)[srcAsInt];
  }

  if (dstType == ConnectType::DINO) {
    dest = dinoDac;
  } else if (dstType == ConnectType::DMA) {
    dest = dma;
  } else {
    dest = (*aurora_channels)[dstAsInt];
  }

  src->connect(src->getDefaultMasterPort(), dest->getDefaultSlavePort());
  if (bidirectional) {
    if (srcType == ConnectType::DINO) {
      src = dinoDac;
    }
    if (dstType == ConnectType::DINO) {
      dest = dinoAdc;
    }
    dest->connect(dest->getDefaultMasterPort(), src->getDefaultSlavePort());
  }
}

void fpga::setupColorHandling() {
  // Handle Control-C nicely
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = [](int) {
    std::cout << std::endl << rang::style::reset << rang::fgB::red;
    std::cout << "Control-C detected, exiting..." << rang::style::reset
              << std::endl;
    std::exit(
        1); // Will call the correct exit func, no unwinding of the stack though
  };

  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, nullptr);

  // Reset color if exiting not by signal
  std::atexit([]() { std::cout << rang::style::reset; });
}

std::shared_ptr<fpga::Card>
fpga::createCard(json_t *config, const std::filesystem::path &searchPath,
                 std::shared_ptr<kernel::vfio::Container> vfioContainer,
                 std::string card_name) {
  auto configDir = std::filesystem::path().parent_path();

  const char *interfaceName;
  json_error_t err;
  logger->info("Found config for FPGA card {}", card_name);
  int ret =
      json_unpack_ex(config, &err, 0, "{s: s}", "interface", &interfaceName);
  if (ret) {
    throw ConfigError(config, err, "interface",
                      "Failed to parse interface name for card {}", card_name);
  }
  std::string interfaceNameStr(interfaceName);
  if (interfaceNameStr == "pcie") {
    auto card = fpga::PCIeCardFactory::make(config, std::string(card_name),
                                            vfioContainer, searchPath);
    if (card) {
      return card;
    }
    return nullptr;
  } else if (interfaceNameStr == "platform") {
    throw RuntimeError("Platform interface not implemented yet");
  } else {
    throw RuntimeError("Unknown interface type {}", interfaceNameStr);
  }
}

int fpga::createCards(json_t *config,
                      std::list<std::shared_ptr<fpga::Card>> &cards,
                      const std::filesystem::path &searchPath,
                      std::shared_ptr<kernel::vfio::Container> vfioContainer) {
  int numFpgas = 0;
  if (vfioContainer == nullptr) {
    vfioContainer = std::make_shared<kernel::vfio::Container>();
  }
  auto configDir = std::filesystem::path().parent_path();

  json_t *fpgas = json_object_get(config, "fpgas");
  if (fpgas == nullptr) {
    logger->error("No section 'fpgas' found in config");
    exit(1);
  }

  const char *card_name;
  json_t *json_card;
  std::shared_ptr<fpga::Card> card;
  json_object_foreach(fpgas, card_name, json_card) {
    card = createCard(json_card, searchPath, vfioContainer, card_name);
    if (card != nullptr) {
      cards.push_back(card);
      numFpgas++;
    }
  }
  return numFpgas;
}

int fpga::createCards(json_t *config,
                      std::list<std::shared_ptr<fpga::Card>> &cards,
                      const std::string &searchPath,
                      std::shared_ptr<kernel::vfio::Container> vfioContainer) {
  const auto fsPath = std::filesystem::path(searchPath);
  return createCards(config, cards, fsPath, vfioContainer);
}

std::shared_ptr<fpga::Card> fpga::setupFpgaCard(const std::string &configFile,
                                                const std::string &fpgaName) {
  auto configDir = std::filesystem::path(configFile).parent_path();

  // Parse FPGA configuration
  FILE *f = fopen(configFile.c_str(), "r");
  if (!f)
    throw RuntimeError("Cannot open config file: {}", configFile);

  json_t *json = json_loadf(f, 0, nullptr);
  if (!json) {
    logger->error("Cannot parse JSON config");
    fclose(f);
    throw RuntimeError("Cannot parse JSON config");
  }

  fclose(f);

  // Create all FPGA card instances using the corresponding plugin
  auto cards = std::list<std::shared_ptr<fpga::Card>>();
  createCards(json, cards, configDir);

  std::shared_ptr<fpga::Card> card = nullptr;
  for (auto &fpgaCard : cards) {
    if (fpgaCard->name == fpgaName) {
      card = fpgaCard;
      break;
    }
  }
  // Deallocate JSON config
  json_decref(json);

  if (!card)
    throw RuntimeError("FPGA card {} not found in config or not working",
                       fpgaName);

  return card;
}

std::unique_ptr<fpga::BufferedSampleFormatter>
fpga::getBufferedSampleFormatter(const std::string &format,
                                 size_t bufSizeInSamples) {
  if (format == "long") {
    return std::make_unique<fpga::BufferedSampleFormatterLong>(
        bufSizeInSamples);
  } else if (format == "short") {
    return std::make_unique<fpga::BufferedSampleFormatterShort>(
        bufSizeInSamples);
  } else {
    throw RuntimeError("Unknown output format '{}'", format);
  }
}
