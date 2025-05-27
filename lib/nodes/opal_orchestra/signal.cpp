/* Helpers for RTAPI signal type conversion.
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 *
 * SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/exceptions.hpp>
#include <villas/nodes/opal_orchestra/signal.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::node::orchestra;

orchestra::SignalType
villas::node::orchestra::toOrchestraSignalType(node::SignalType t) {
  switch (t) {
  case node::SignalType::FLOAT:
    return orchestra::SignalType::FLOAT64;
  case node::SignalType::INTEGER:
    return orchestra::SignalType::INT64;
  case node::SignalType::BOOLEAN:
    return orchestra::SignalType::BOOLEAN;
  default:
    throw RuntimeError("Unsupported signal type: {}", signalTypeToString(t));
  }
}

node::SignalType
villas::node::orchestra::toNodeSignalType(orchestra::SignalType t) {
  switch (t) {
  case orchestra::SignalType::FLOAT32:
  case orchestra::SignalType::FLOAT64:
    return node::SignalType::FLOAT;

  case orchestra::SignalType::INT8:
  case orchestra::SignalType::UINT8:
  case orchestra::SignalType::INT16:
  case orchestra::SignalType::UINT16:
  case orchestra::SignalType::INT32:
  case orchestra::SignalType::UINT32:
  case orchestra::SignalType::INT64:
  case orchestra::SignalType::UINT64:
    return node::SignalType::INTEGER;

  case orchestra::SignalType::BOOLEAN:
    return node::SignalType::BOOLEAN;

  default:
    throw RuntimeError("Unsupported Orchestra signal type: {}", (int)t);
  }
}

orchestra::SignalType
villas::node::orchestra::signalTypeFromString(const std::string &t) {
  if (t == "boolean") {
    return orchestra::SignalType::BOOLEAN;
  } else if (t == "unsigned int8") {
    return orchestra::SignalType::UINT8;
  } else if (t == "unsigned int16") {
    return orchestra::SignalType::UINT16;
  } else if (t == "unsigned int32") {
    return orchestra::SignalType::UINT32;
  } else if (t == "unsigned int64") {
    return orchestra::SignalType::UINT64;
  } else if (t == "int8") {
    return orchestra::SignalType::INT8;
  } else if (t == "int16") {
    return orchestra::SignalType::INT16;
  } else if (t == "int32") {
    return orchestra::SignalType::INT32;
  } else if (t == "int64") {
    return orchestra::SignalType::INT64;
  } else if (t == "float32") {
    return orchestra::SignalType::FLOAT32;
  } else if (t == "float64") {
    return orchestra::SignalType::FLOAT64;
  } else if (t == "bus") {
    return orchestra::SignalType::BUS;
  } else {
    throw RuntimeError("Unknown Orchestra signal type: {}", t);
  }
}

std::string
villas::node::orchestra::signalTypeToString(orchestra::SignalType t) {
  switch (t) {
  case orchestra::SignalType::BOOLEAN:
    return "boolean";
  case orchestra::SignalType::UINT8:
    return "unsigned int8";
  case orchestra::SignalType::UINT16:
    return "unsigned int16";
  case orchestra::SignalType::UINT32:
    return "unsigned int32";
  case orchestra::SignalType::UINT64:
    return "unsigned int64";
  case orchestra::SignalType::INT8:
    return "int8";
  case orchestra::SignalType::INT16:
    return "int16";
  case orchestra::SignalType::INT32:
    return "int32";
  case orchestra::SignalType::INT64:
    return "int64";
  case orchestra::SignalType::FLOAT32:
    return "float32";
  case orchestra::SignalType::FLOAT64:
    return "float64";
  case orchestra::SignalType::BUS:
    return "bus";
  default:
    throw RuntimeError("Unknown Orchestra signal type: {}", (int)t);
  }
}

SignalData
villas::node::orchestra::toNodeSignalData(const char *orchestraData,
                                          orchestra::SignalType orchestraType,
                                          node::SignalType &villasType) {

  villasType = toNodeSignalType(orchestraType);

  switch (orchestraType) {
  case orchestra::SignalType::BOOLEAN:
    return *reinterpret_cast<const bool *>(orchestraData);

  case orchestra::SignalType::UINT8:
    return *reinterpret_cast<const uint8_t *>(orchestraData);

  case orchestra::SignalType::UINT16:
    return *reinterpret_cast<const uint16_t *>(orchestraData);

  case orchestra::SignalType::UINT32:
    return *reinterpret_cast<const uint32_t *>(orchestraData);

  case orchestra::SignalType::UINT64:
    return *reinterpret_cast<const uint64_t *>(orchestraData);

  case orchestra::SignalType::INT8:
    return *reinterpret_cast<const int8_t *>(orchestraData);

  case orchestra::SignalType::INT16:
    return *reinterpret_cast<const int16_t *>(orchestraData);

  case orchestra::SignalType::INT32:
    return *reinterpret_cast<const int32_t *>(orchestraData);

  case orchestra::SignalType::INT64:
    return *reinterpret_cast<const int64_t *>(orchestraData);

  case orchestra::SignalType::FLOAT32:
    return *reinterpret_cast<const float *>(orchestraData);

  case orchestra::SignalType::FLOAT64:
    return *reinterpret_cast<const double *>(orchestraData);

  default:
    throw RuntimeError("Orchestra signal type {} is not supported",
                       signalTypeToString(orchestraType));
  };

  return SignalData(); // Unreachable
}

void villas::node::orchestra::toOrchestraSignalData(
    char *orchestraData, orchestra::SignalType orchestraType,
    const SignalData &villasData, node::SignalType villasType) {
  auto villasTypeCasted = toNodeSignalType(orchestraType);
  auto villasDataCasted = villasData.cast(villasType, villasTypeCasted);

  switch (orchestraType) {
  case orchestra::SignalType::BOOLEAN:
    *reinterpret_cast<bool *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::UINT8:
    *reinterpret_cast<uint8_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::UINT16:
    *reinterpret_cast<uint16_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::UINT32:
    *reinterpret_cast<uint32_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::UINT64:
    *reinterpret_cast<uint64_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::INT8:
    *reinterpret_cast<int8_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::INT16:
    *reinterpret_cast<int16_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::INT32:
    *reinterpret_cast<int32_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::INT64:
    *reinterpret_cast<int64_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::FLOAT32:
    *reinterpret_cast<float *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::FLOAT64:
    *reinterpret_cast<double *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::BUS:
    throw RuntimeError("Bus signal type not supported");
  }
}
