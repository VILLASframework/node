#pragma once

#include <memory>
#include <string>

#include <nlohmann/json_fwd.hpp>

struct json_t;

namespace villas {

class config_json_base;

using config_json =
    nlohmann::basic_json<std::map,                  // ObjectType
                         std::vector,               // ArrayType
                         std::string,               // StringType
                         bool,                      // BooleanType
                         std::int64_t,              // NumberIntegerType
                         std::uint64_t,             // NumberUnsignedType
                         double,                    // NumberFloatType
                         std::allocator,            // AllocatorType
                         nlohmann::adl_serializer,  // JSONSerializer
                         std::vector<std::uint8_t>, // BinaryType
                         config_json_base           // CustomBaseClass
                         >;

using config_json_pointer = nlohmann::json_pointer<std::string>;

}; // namespace villas
