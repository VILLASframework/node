/* OPAL-RT Orchestra Data Definition File (DDF)
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 *
 * SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <libxml/tree.h>

#include <villas/nodes/opal_orchestra/signal.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>
#include <villas/signal_list.hpp>

namespace villas {
namespace node {
namespace orchestra {

class Item {
public:
  virtual void toXml(xmlNode *parent, bool withDefault) const = 0;
};

class DataItem : public Item {

public:
  // Config-time members.
  std::string name;
  SignalType type;
  unsigned short length;
  double defaultValue;

  DataItem(const std::string &name) : name(name) {}

  static const unsigned int IDENTIFIER_NAME_LENGTH = 64;

  void toXml(xmlNode *parent, bool withDefault) const override;
};

class BusItem : public Item {
public:
  std::map<std::string, std::shared_ptr<Item>> items;
  std::string name;

  BusItem(const std::string &name) : items(), name(name) {}

  std::shared_ptr<DataItem> upsertItem(std::vector<std::string> pathComponents,
                                       bool &inserted);

  void toXml(xmlNode *parent, bool withDefault) const override;
};

class DataSet {
public:
  std::map<std::string, std::shared_ptr<Item>> items;
  std::string name;

  DataSet(const std::string &name) : items(), name(name) {}

  std::shared_ptr<DataItem> upsertItem(const std::string &path, bool &inserted);
  std::shared_ptr<DataItem> upsertItem(std::vector<std::string> pathComponents,
                                       bool &inserted);

  void toXml(xmlNode *parent, bool withDefault) const;
};

class Connection {
public:
  virtual void toXml(xmlNode *domain) const = 0;
  virtual std::string getDetails() const = 0;
  virtual void parse(json_t *json) = 0;

  static std::shared_ptr<Connection> fromJson(json_t *json);
};

class ConnectionLocal : public Connection {
public:
  std::optional<std::string> extcomm;
  std::optional<std::string> addressFramework;
  std::optional<uint16_t> portFramework;
  std::optional<std::string> nicFramework;
  std::optional<std::string> nicClient;
  std::optional<int> coreFramework;
  std::optional<int> coreClient;

  std::string getDetails() const override;

  void toXml(xmlNode *domain) const override;

  void parse(json_t *json) override;
};

class ConnectionRemote : public Connection {
public:
  std::string name; // The reflective memory card name.
  int pciIndex; // The index of the card attributed when the PCI bus is scanned. If there is only one card, this index must be 0.

  std::string getDetails() const override;

  void toXml(xmlNode *domain) const override;

  void parse(json_t *json) override;
};

class ConnectionDolphin : public Connection {
public:
  int nodeIdFramework;
  int segmentId;

  std::string getDetails() const override;

  void toXml(xmlNode *domain) const override;

  void parse(json_t *json) override;
};

class Domain {
public:
  std::string name;
  std::shared_ptr<Connection> connection;
  bool synchronous;
  bool states;
  bool multiplePublishAllowed;

  DataSet
      publish; // Signals published by the Orchestra framework to VILLASnode.
  DataSet
      subscribe; // Signals subscribed by VILLASnode from the Orchestra framework.

  Domain()
      : name("villas-node"), connection(new ConnectionLocal{}),
        synchronous(false), states(false), multiplePublishAllowed(false),
        publish("PUBLISH"), subscribe("SUBSCRIBE") {}

  void toXml(xmlNode *parent) const;
};

class DataDefinitionFile {
public:
  std::vector<Domain> domains;

  xmlNode *toXml() const;

  void writeToFile(const std::filesystem::path &filename) const;
};

} // namespace orchestra
} // namespace node
} // namespace villas
