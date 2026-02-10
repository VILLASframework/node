#pragma once

#include <memory>
#include <span>

#include <fmt/ostream.h>
#include <nlohmann/json.hpp>

#include <villas/fs.hpp>
#include <villas/json_fwd.hpp>

// libjansson forward declaration
struct json_t;

namespace villas {

// deleter for libjansson ::json_t * values
struct libjansson_deleter {
  void operator()(::json_t *) const;
};

// smart pointer for libjansson ::json_t * values.
using libjansson_ptr = std::unique_ptr<::json_t, libjansson_deleter>;

// base class which injects VILLASnode specific functionality
// into the config_json specialization of nlohmann::basic_json.
class config_json_base {
private:
  friend config_json;
  config_json_base() = default;

public:
  // libjansson compatability
  static config_json from_libjansson(::json_t const *);
  libjansson_ptr to_libjansson() const;

  // configuration parsing options
  struct options_t {
    // expand $ENV variable substitutions
    bool expand_substitutions = false;
    // expand $include keys
    bool expand_includes = false;
    // expand deprecated ${ENV} substitutions
    bool expand_deprecated = false;
    // a set of base directories to search for includes
    std::span<const fs::path> include_directories = {};
  };

  // load a VILLASnode configuration
  static config_json load_config(std::FILE *, options_t);
  static config_json load_config(std::string_view, options_t);
  static config_json load_config_file(fs::path const &, options_t);
};

}; // namespace villas

template <> // format config_json using operator<<
struct fmt::formatter<villas::config_json> : ostream_formatter {};

template <> // format config_json_pointer using operator<<
struct fmt::formatter<villas::config_json_pointer> : ostream_formatter {};
