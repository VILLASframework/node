/* Helpers for RTAPI signal type conversion.
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 *
 * SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/exceptions.hpp>
#include <villas/nodes/orchestra/signal.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::node::orchestra;

orchestra::SignalType
villas::node::orchestra::toOrchestraSignalType(node::SignalType t) {
  switch (t) {
  case node::SignalType::FLOAT:
    return orchestra::SignalType::Float64;
  case node::SignalType::INTEGER:
    return orchestra::SignalType::Int64;
  case node::SignalType::BOOLEAN:
    return orchestra::SignalType::Boolean;
  default:
    throw RuntimeError("Unsupported signal type: {}", signalTypeToString(t));
  }
}

node::SignalType
villas::node::orchestra::toNodeSignalType(orchestra::SignalType t) {
  switch (t) {
  case orchestra::SignalType::Float32:
  case orchestra::SignalType::Float64:
    return node::SignalType::FLOAT;

  case orchestra::SignalType::Int8:
  case orchestra::SignalType::UInt8:
  case orchestra::SignalType::Int16:
  case orchestra::SignalType::UInt16:
  case orchestra::SignalType::Int32:
  case orchestra::SignalType::UInt32:
  case orchestra::SignalType::Int64:
  case orchestra::SignalType::UInt64:
    return node::SignalType::INTEGER;

  case orchestra::SignalType::Boolean:
    return node::SignalType::BOOLEAN;

  default:
    throw RuntimeError("Unsupported Orchestra signal type: {}", (int)t);
  }
}

orchestra::SignalType
villas::node::orchestra::signalTypeFromString(const std::string &t) {
  if (t == "boolean") {
    return orchestra::SignalType::Boolean;
  } else if (t == "unsigned int8") {
    return orchestra::SignalType::UInt8;
  } else if (t == "unsigned int16") {
    return orchestra::SignalType::UInt16;
  } else if (t == "unsigned int32") {
    return orchestra::SignalType::UInt32;
  } else if (t == "unsigned int64") {
    return orchestra::SignalType::UInt64;
  } else if (t == "int8") {
    return orchestra::SignalType::Int8;
  } else if (t == "int16") {
    return orchestra::SignalType::Int16;
  } else if (t == "int32") {
    return orchestra::SignalType::Int32;
  } else if (t == "int64") {
    return orchestra::SignalType::Int64;
  } else if (t == "float32") {
    return orchestra::SignalType::Float32;
  } else if (t == "float64") {
    return orchestra::SignalType::Float64;
  } else if (t == "bus") {
    return orchestra::SignalType::Bus;
  } else {
    throw RuntimeError("Unknown Orchestra signal type: {}", t);
  }
}

std::string
villas::node::orchestra::signalTypeToString(orchestra::SignalType t) {
  switch (t) {
  case orchestra::SignalType::Boolean:
    return "boolean";
  case orchestra::SignalType::UInt8:
    return "unsigned int8";
  case orchestra::SignalType::UInt16:
    return "unsigned int16";
  case orchestra::SignalType::UInt32:
    return "unsigned int32";
  case orchestra::SignalType::UInt64:
    return "unsigned int64";
  case orchestra::SignalType::Int8:
    return "int8";
  case orchestra::SignalType::Int16:
    return "int16";
  case orchestra::SignalType::Int32:
    return "int32";
  case orchestra::SignalType::Int64:
    return "int64";
  case orchestra::SignalType::Float32:
    return "float32";
  case orchestra::SignalType::Float64:
    return "float64";
  case orchestra::SignalType::Bus:
    return "bus";
  default:
    throw RuntimeError("Unknown Orchestra signal type: {}", (int)t);
  }
}

SignalData
villas::node::orchestra::toNodeSignalData(const void *orchestraData,
                                          orchestra::SignalType orchestraType,
                                          node::SignalType &villasType) {

  villasType = toNodeSignalType(orchestraType);

  switch (orchestraType) {
  case orchestra::SignalType::Boolean:
    return *reinterpret_cast<const bool *>(orchestraData);

  case orchestra::SignalType::UInt8:
    return *reinterpret_cast<const uint8_t *>(orchestraData);

  case orchestra::SignalType::UInt16:
    return *reinterpret_cast<const uint16_t *>(orchestraData);

  case orchestra::SignalType::UInt32:
    return *reinterpret_cast<const uint32_t *>(orchestraData);

  case orchestra::SignalType::UInt64:
    return *reinterpret_cast<const uint64_t *>(orchestraData);

  case orchestra::SignalType::Int8:
    return *reinterpret_cast<const int8_t *>(orchestraData);

  case orchestra::SignalType::Int16:
    return *reinterpret_cast<const int16_t *>(orchestraData);

  case orchestra::SignalType::Int32:
    return *reinterpret_cast<const int32_t *>(orchestraData);

  case orchestra::SignalType::Int64:
    return *reinterpret_cast<const int64_t *>(orchestraData);

  case orchestra::SignalType::Float32:
    return *reinterpret_cast<const float *>(orchestraData);

  case orchestra::SignalType::Float64:
    return *reinterpret_cast<const double *>(orchestraData);

  case orchestra::SignalType::Bus:
    throw RuntimeError("Orchestra signal type {} is not supported",
                       signalTypeToString(orchestraType));
  };

  return SignalData(); // Should never be reached, but avoids compiler warning.
}

void villas::node::orchestra::toOrchestraSignalData(
    void *orchestraData, orchestra::SignalType orchestraType,
    const SignalData &villasData, node::SignalType villasType) {
  auto villasTypeCasted = toNodeSignalType(orchestraType);
  auto villasDataCasted = villasData.cast(villasType, villasTypeCasted);

  switch (orchestraType) {
  case orchestra::SignalType::Boolean:
    *reinterpret_cast<bool *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::UInt8:
    *reinterpret_cast<uint8_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::UInt16:
    *reinterpret_cast<uint16_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::UInt32:
    *reinterpret_cast<uint32_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::UInt64:
    *reinterpret_cast<uint64_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::Int8:
    *reinterpret_cast<int8_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::Int16:
    *reinterpret_cast<int16_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::Int32:
    *reinterpret_cast<int32_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::Int64:
    *reinterpret_cast<int64_t *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::Float32:
    *reinterpret_cast<float *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::Float64:
    *reinterpret_cast<double *>(orchestraData) = villasDataCasted;
    break;

  case orchestra::SignalType::Bus:
    throw RuntimeError("Bus signal type not supported");
  }
}
