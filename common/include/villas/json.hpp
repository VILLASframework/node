#pragma once

#include <fmt/ostream.h>
#include <nlohmann/json_fwd.hpp>

#include <villas/fs.hpp>

namespace villas {

using Json = nlohmann::json;
using JsonPointer = nlohmann::json_pointer<std::string>;

// forward declaration for villas/jansson.hpp compatibility header
class JanssonPtr;
void to_json(Json &json, JanssonPtr const &jansson);
void from_json(Json const &json, JanssonPtr &jansson);

// load a configuration file
Json load_config_deprecated(fs::path const &path, bool resolve_inc,
                            bool resolve_env);

}; // namespace villas

// forward declaration for libjansson's json_t
struct json_t;
void to_json(villas::Json &json, json_t const *jansson);

template <> // format config_json using operator<<
struct fmt::formatter<villas::Json> : ostream_formatter {};

template <> // format config_json_pointer using operator<<
struct fmt::formatter<villas::JsonPointer> : ostream_formatter {};
