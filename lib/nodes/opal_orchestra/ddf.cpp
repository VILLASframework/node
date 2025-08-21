/* OPAL-RT Orchestra Data Definition File (DDF)
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 *
 * SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>
extern "C" {
#include <RTAPI.h>
}

#include <villas/nodes/opal_orchestra/ddf.hpp>
#include <villas/nodes/opal_orchestra/error.hpp>
#include <villas/nodes/opal_orchestra/signal.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>

using namespace villas::node;
using namespace villas::node::orchestra;

void DataItem::toXml(xmlNode *parent, bool withDefault) const {
  xmlNode *item = xmlNewChild(parent, nullptr, BAD_CAST "item", nullptr);
  xmlNewProp(item, BAD_CAST "name", BAD_CAST name.c_str());
  xmlNewChild(item, nullptr, BAD_CAST "type",
              BAD_CAST signalTypeToString(type).c_str());
  xmlNewChild(item, nullptr, BAD_CAST "length",
              BAD_CAST fmt::format("{}", length).c_str());

  if (withDefault) {
    std::string defaultValueStr;
    switch (type) {
    case orchestra::SignalType::BOOLEAN:
      defaultValueStr = defaultValue ? "yes" : "no";
      break;

    default:
      defaultValueStr = fmt::format("{}", defaultValue);
      break;
    }

    xmlNewChild(item, nullptr, BAD_CAST "default",
                BAD_CAST defaultValueStr.c_str());
  }
}

void BusItem::toXml(xmlNode *parent, bool withDefault) const {
  xmlNode *node = xmlNewChild(parent, NULL, BAD_CAST "item", NULL);
  xmlNewProp(node, BAD_CAST "name", BAD_CAST name.c_str());
  xmlNewChild(node, NULL, BAD_CAST "type", BAD_CAST "bus");

  for (const auto &p : items) {
    auto &item = p.second;
    item->toXml(node, withDefault);
  }
}

std::shared_ptr<DataItem> BusItem::upsertItem(std::string_view path,
                                              bool &inserted) {
  auto separator = path.find('/');
  auto isSignal = separator == std::string::npos; // No bus, just a signal.
  auto name = std::string(path.substr(0, separator));

  auto it = items.find(name);
  if (it == items.end()) { // No item with this name exists. Create a new one.
    if (isSignal) {
      auto item = std::make_shared<DataItem>(name);
      items.emplace(name, item);
      inserted = true;
      return item;
    } else {
      auto bus = std::make_shared<BusItem>(name);
      items.emplace(name, bus);

      return bus->upsertItem(path.substr(separator + 1), inserted);
    }
  } else {
    if (isSignal) {
      auto item = std::dynamic_pointer_cast<DataItem>(it->second);
      if (!item) {
        throw RuntimeError("Item with name '{}' is not a data item", name);
      }

      inserted = false;
      return item;
    } else {
      auto bus = std::dynamic_pointer_cast<BusItem>(it->second);
      if (!bus) {
        throw RuntimeError("Item with name '{}' is not a bus", name);
      }

      return bus->upsertItem(path.substr(separator + 1), inserted);
    }
  }
}

std::shared_ptr<DataItem> DataSet::upsertItem(std::string_view path,
                                              bool &inserted) {
  auto separator = path.find('/');
  auto name = std::string(path.substr(0, separator));
  auto it = items.find(name);
  if (it == items.end()) { // No item with this name exists. Create a new one.
    if (separator == std::string_view::npos) { // No bus, just a signal.
      auto item = std::make_shared<DataItem>(name);
      items.emplace(std::move(name), item);
      inserted = true;
      return item;
    } else {
      auto bus = std::make_shared<BusItem>(name);
      items.emplace(std::move(name), bus);
      return bus->upsertItem(path.substr(separator + 1), inserted);
    }
  } else {
    if (separator == std::string_view::npos) {
      auto item = std::dynamic_pointer_cast<DataItem>(it->second);
      if (!item) {
        throw RuntimeError("Item with name '{}' is not a data item", name);
      }
      inserted = false;
      return item;
    } else {
      // Item with this name exists. Check if it is a bus.
      auto bus = std::dynamic_pointer_cast<BusItem>(it->second);
      if (!bus) {
        throw RuntimeError("Item with name '{}' is not a bus", name);
      }
      return bus->upsertItem(path.substr(separator + 1), inserted);
    }
  }
}

void DataSet::toXml(xmlNode *parent, bool withDefault) const {
  xmlNode *node = xmlNewChild(parent, NULL, BAD_CAST "set", NULL);
  xmlNewProp(node, BAD_CAST "name", BAD_CAST name.c_str());

  for (const auto &p : items) {
    auto &item = p.second;
    item->toXml(node, withDefault);
  }
}

std::string ConnectionLocal::getDetails() const {
  auto details = fmt::format("type=local");
  if (extcomm) {
    details += fmt::format(", extcomm={}", *extcomm);

    if (addressFramework) {
      details += fmt::format(", addr_framework={}", *addressFramework);
    }
    if (portFramework) {
      details += fmt::format(", port_framework={}", portFramework.value());
    }
    if (nicFramework) {
      details += fmt::format(", nic_framework={}", *nicFramework);
    }
    if (nicClient) {
      details += fmt::format(", nic_client={}", *nicClient);
    }
    if (coreFramework) {
      details += fmt::format(", core_framework={}", coreFramework.value());
    }
    if (coreClient) {
      details += fmt::format(", core_client={}", coreClient.value());
    }
  }
  details += " }";

  return details;
}

void ConnectionLocal::toXml(xmlNode *domain) const {
  xmlNode *connection =
      xmlNewChild(domain, nullptr, BAD_CAST "connection", nullptr);
  xmlNewProp(connection, BAD_CAST "type", BAD_CAST "local");
  if (extcomm) {
    xmlNewProp(connection, BAD_CAST "extcomm", BAD_CAST extcomm->c_str());

    if (addressFramework) {
      xmlNewProp(connection, BAD_CAST "addrframework",
                 BAD_CAST addressFramework->c_str());
    }
    if (portFramework) {
      xmlNewProp(connection, BAD_CAST "portframework",
                 BAD_CAST fmt::format("{}", portFramework.value()).c_str());
    }
    if (nicFramework) {
      xmlNewProp(connection, BAD_CAST "nicframework",
                 BAD_CAST nicFramework->c_str());
    }
    if (nicClient) {
      xmlNewProp(connection, BAD_CAST "nicclient", BAD_CAST nicClient->c_str());
    }
    if (coreFramework) {
      xmlNewProp(connection, BAD_CAST "coreframework",
                 BAD_CAST fmt::format("{}", coreFramework.value()).c_str());
    }
    if (coreClient) {
      xmlNewProp(connection, BAD_CAST "coreclient",
                 BAD_CAST fmt::format("{}", coreClient.value()).c_str());
    }
  }
}

void ConnectionLocal::parse(json_t *json) {
  int pf = -1;
  int cf = -1;
  int cc = -1;
  const char *ec = nullptr;
  const char *af = nullptr;
  const char *nf = nullptr;
  const char *nc = nullptr;

  json_error_t err;

  auto ret = json_unpack_ex(
      json, &err, 0, "{ s?: s, s?: s, s?: i, s?: s, s?: s, s?: i, s?: i }",
      "extcomm", &ec, "addr_framework", &af, "port_framework", &pf,
      "nic_framework", &nf, "nic_client", &nc, "core_framework", &cf,
      "core_client", &cc);
  if (ret) {
    throw ConfigError(json, err, "node-config-node-opal-orchestra-connection",
                      "Failed to parse configuration of local connection");
  }

  if (ec) {
    extcomm = ec;
  }
  if (af) {
    addressFramework = af;
  }
  if (pf >= 0) {
    portFramework = pf;
  }
  if (nf) {
    nicFramework = nf;
  }
  if (nc) {
    nicClient = nc;
  }
  if (cf >= 0) {
    coreFramework = cf;
  }
  if (cc >= 0) {
    coreClient = cc;
  }
}

std::string ConnectionRemote::getDetails() const {
  return fmt::format(", connection={{ type=remote, name={}, pci_index={} }}",
                     name, pciIndex);
}

void ConnectionRemote::toXml(xmlNode *domain) const {
  xmlNode *connection =
      xmlNewChild(domain, nullptr, BAD_CAST "remote", nullptr);
  xmlNewProp(connection, BAD_CAST "type", BAD_CAST "local");
  xmlNewProp(connection, BAD_CAST "type", BAD_CAST "remote");
  xmlNewChild(connection, nullptr, BAD_CAST "card", BAD_CAST name.c_str());
  xmlNewChild(connection, nullptr, BAD_CAST "pciindex",
              BAD_CAST fmt::format("{}", pciIndex).c_str());
}

void ConnectionRemote::parse(json_t *json) {
  const char *nme;

  json_error_t err;

  int ret = json_unpack_ex(json, &err, 0, "{ s: s, s: i }", "card", &nme,
                           "pci_index", &pciIndex);
  if (ret) {
    throw ConfigError(
        json, err, "node-config-node-opal-orchestra-rfm",
        "Failed to parse configuration of Reflective Memory Card (RFM)");
  }

  name = nme;
}

std::string ConnectionDolphin::getDetails() const {
  return fmt::format(
      ", connection={{ type=dolphin, node_id_framework={}, segment_id={} }}",
      nodeIdFramework, segmentId);
}

void ConnectionDolphin::toXml(xmlNode *domain) const {
  xmlNode *connection =
      xmlNewChild(domain, nullptr, BAD_CAST "dolphin", nullptr);
  xmlNewProp(connection, BAD_CAST "type", BAD_CAST "local");
  xmlNewProp(connection, BAD_CAST "type", BAD_CAST "dolphin");
  xmlNewProp(connection, BAD_CAST "nodeIdFramework",
             BAD_CAST fmt::format("{}", nodeIdFramework).c_str());
  xmlNewProp(connection, BAD_CAST "segmentId",
             BAD_CAST fmt::format("{}", segmentId).c_str());
}

void ConnectionDolphin::parse(json_t *json) {
  json_error_t err;
  auto ret =
      json_unpack_ex(json, &err, 0, "{ s: i, s: i }", "node_id_framework",
                     &nodeIdFramework, "segment_id", &segmentId);
  if (ret) {
    throw ConfigError(json, err, "node-config-node-opal-orchestra-connection",
                      "Failed to parse configuration of dolphin connection");
  }
}

std::shared_ptr<Connection> Connection::fromJson(json_t *json) {
  const char *type = nullptr;
  json_error_t err;

  int ret = json_unpack_ex(json, &err, 0, "{ s: s, }", "type", &type);
  if (ret) {
    throw ConfigError(json, err, "node-config-node-opal-orchestra-connection",
                      "Failed to parse connection type");
  }

  std::shared_ptr<Connection> c;

  if (strcmp(type, "local") == 0) {
    c = std::make_shared<ConnectionLocal>();
    c->parse(json);
  } else if (strcmp(type, "remote") == 0) {
    c = std::make_shared<ConnectionRemote>();
    c->parse(json);
  } else if (strcmp(type, "dolphin") == 0) {
    c = std::make_shared<ConnectionDolphin>();
    c->parse(json);
  } else {
    throw ConfigError(json, err, "node-config-node-opal-orchestra-connection",
                      "Unknown type of connection '{}'", type);
  }

  return c;
}

void Domain::toXml(xmlNode *parent) const {
  // Create the <domain> node with attributes
  xmlNode *domain = xmlNewChild(parent, nullptr, BAD_CAST "domain", nullptr);
  xmlNewProp(domain, BAD_CAST "name", BAD_CAST name.c_str());

  // Add child nodes to the domain
  xmlNewChild(domain, nullptr, BAD_CAST "synchronous",
              BAD_CAST(synchronous ? "yes" : "no"));
  xmlNewChild(domain, nullptr, BAD_CAST "multiplePublishAllowed",
              BAD_CAST(multiplePublishAllowed ? "yes" : "no"));
  xmlNewChild(domain, nullptr, BAD_CAST "states",
              BAD_CAST(states ? "yes" : "no"));

  connection->toXml(domain);

  publish.toXml(
      domain,
      false); // Add the PUBLISH set (input signals from Orchestra clients (VILLASnode's) perspective)
  subscribe.toXml(
      domain,
      true); // Add the SUBSCRIBE set (output signals from Orchestra clients (VILLASnode's) perspective)
}

xmlNode *DataDefinitionFile::toXml() const {
  xmlNode *rootNode = xmlNewNode(nullptr, BAD_CAST "orchestra");

  for (const auto &domain : domains) {
    domain.toXml(rootNode);
  }

  return rootNode;
}

void DataDefinitionFile::writeToFile(
    const std::filesystem::path &filename) const {
  // Create a new XML document
  auto *doc = xmlNewDoc(BAD_CAST "1.0");
  auto *rootNode = toXml();

  xmlDocSetRootElement(doc, rootNode);

  // Save the document to a file
  if (xmlSaveFormatFileEnc(filename.c_str(), doc, "UTF-8", 1) == -1) {
    xmlFreeDoc(doc);
    throw RuntimeError("Failed to save XML file");
  }
}
