#include <cstring>
#include <filesystem>
#include <ranges>
#include <regex>
#include <stdexcept>
#include <system_error>
#include <vector>

#include <fmt/format.h>

extern "C" {
#include <fnmatch.h>
#include <jansson.h>
#include <libconfig.h>
}

#include <villas/json.hpp>
#include <villas/log.hpp>
#include <villas/utils.hpp>

using namespace std::string_view_literals;

namespace villas {

void libjansson_deleter::operator()(::json_t *json) const { json_decref(json); }

config_json config_json_base::from_libjansson(::json_t const *json) {
  switch (json_typeof(json)) {
    using enum json_type;

  case JSON_ARRAY: {
    std::size_t index;
    ::json_t *value;

    auto array = config_json::array();
    json_array_foreach (json, index, value)
      array.push_back(config_json::from_libjansson(value));

    return array;
  }

  case JSON_OBJECT: {
    char const *key;
    std::size_t keylen;
    ::json_t *value;

    auto object = config_json::object();
    // The const_cast below is safe as long as we don't replace the contained value by
    // accessing the underlying iterator with json_object_key_to_iter(key) and
    // json_object_iter_set or json_object_iter_set_new.
    //
    // There is no API in libjansson to enumerate keys of a `::json_t const *` object.
    //
    // See https://github.com/akheron/jansson/issues/578
    json_object_keylen_foreach (const_cast<::json_t *>(json), key, keylen,
                                value)
      object.emplace(std::string{key, keylen},
                     config_json::from_libjansson(value));

    return object;
  }

  case JSON_STRING: {
    return std::string{json_string_value(json), json_string_length(json)};
  }

  case JSON_INTEGER: {
    return json_integer_value(json);
  }

  case JSON_REAL: {
    return json_real_value(json);
  }

  case JSON_TRUE: {
    return true;
  }

  case JSON_FALSE: {
    return false;
  }

  case JSON_NULL:
  default: {
    return nullptr;
  }
  }
}

libjansson_ptr config_json_base::to_libjansson() const {
  // The static_cast below assumes that the *this object is actually a config_json
  // object which is derived from config_json_base. This is ensured by the private
  // constructor of `config_json_base` which makes config_json_base only constructible
  // as part of a friend config_json instance.
  //
  // Read up on the CRTP C++ design pattern for more information.
  auto const &self = *static_cast<config_json const *>(this);

  switch (self.type()) {
    using value_t = config_json::value_t;

  case value_t::array: {
    auto array = libjansson_ptr(json_array());
    for (auto const &item : self)
      json_array_append_new(array.get(), item.to_libjansson().release());

    return array;
  }

  case value_t::object: {
    auto object = libjansson_ptr(json_object());
    for (auto const &[key, value] : self.items())
      json_object_setn_new_nocheck(object.get(), key.data(), key.size(),
                                   value.to_libjansson().release());

    return object;
  }

  case value_t::string: {
    auto const string = self.get<config_json::string_t const *>();
    return libjansson_ptr(json_stringn_nocheck(string->data(), string->size()));
  }

  case value_t::number_integer: {
    auto const integer = self.get<config_json::number_integer_t const *>();
    return libjansson_ptr(json_integer(*integer));
  }

  case value_t::number_unsigned: {
    auto const integer = self.get<config_json::number_unsigned_t const *>();
    return libjansson_ptr(json_integer(*integer));
  }

  case value_t::number_float: {
    auto const real = self.get<config_json::number_float_t const *>();
    return libjansson_ptr(json_real(*real));
  }

  case value_t::boolean: {
    auto const boolean = self.get<config_json::boolean_t const *>();
    return libjansson_ptr(json_boolean(*boolean));
  }

  case value_t::null:
    return libjansson_ptr(json_null());

  case value_t::binary:
    throw std::invalid_argument{"cannot convert binary value to libjansson"};

  case value_t::discarded:
    throw std::invalid_argument{"cannot convert discarded value to libjansson"};

  default:
    __builtin_unreachable();
  }
}

namespace {

// implementation of deprecated variable substitutions
bool deprecatedSubstitution(config_json &value,
                            config_json::options_t options) {
  if (not options.expand_deprecated or not value.is_string())
    return false;

  constexpr static auto COMPAT_INCLUDE_KEYWORD = "@include "sv;
  auto logger = Log::get("config");
  auto string = value.get_ref<std::string &>();
  auto doInclude = false;
  auto expanded = std::size_t{0};

  // check for legacy @include keyword
  if (options.expand_includes and string.starts_with(COMPAT_INCLUDE_KEYWORD)) {
    doInclude = true;
    expanded = COMPAT_INCLUDE_KEYWORD.length();
  }

  // legacy environment variable substitution syntax
  static auto const envRegex = std::regex(R"--(\$\{([^}]+)\})--");
  enum : std::size_t {
    CAPTURE_ALL = 0,
    CAPTURE_NAME,
  };

  // expand legacy environment substition syntax
  std::smatch match;
  while (options.expand_substitutions and
         std::regex_search(string.cbegin() + expanded, string.cend(), match,
                           envRegex)) {
    auto name = std::string(match[CAPTURE_NAME]);
    auto envPtr = std::getenv(name.c_str());
    if (not envPtr)
      throw std::runtime_error(
          fmt::format("Could substitute environment variable {:?}", name));

    auto envValue = std::string_view(envPtr);
    auto [begin, end] = std::pair(match[CAPTURE_ALL]);
    string.replace(begin, end, envValue.begin(), envValue.end());
    expanded += match.position() + envValue.length();
  }

  // expand legacy @include directive
  if (doInclude) {
    auto pattern =
        std::string_view(string).substr(COMPAT_INCLUDE_KEYWORD.length());
    auto result = config_json(nullptr);
    for (auto path : utils::glob(pattern, options.include_directories)) {
      auto partial_result = config_json::load_config_file(path, options);
      if (result.is_null())
        result = partial_result;
      else if (partial_result.is_object() and result.is_object())
        result.update(partial_result, true);
      else if (partial_result.is_array() and result.is_array())
        result.insert(result.end(), partial_result.begin(),
                      partial_result.end());
    }

    logger->warn("Found deprecated @include directive: {}", value);
    value = std::move(result);
  } else if (expanded) {
    logger->warn("Found deprecated environment substitution: {}", value);
    value = std::move(string);
  }

  return expanded != 0;
}

void expandStringSubstitution(std::string &string) {
  if (not string.starts_with("$"sv))
    return;

  if (string.starts_with("$$"sv)) {
    string.erase(0);
    return;
  }

  constexpr static auto STRING_PREFIX = "$string:"sv;
  if (string.starts_with(STRING_PREFIX)) {
    auto name = string.c_str() + STRING_PREFIX.length();
    auto env = std::getenv(name);
    if (not env)
      throw std::runtime_error(fmt::format(
          "Failed to substitute unknown environment variable {:?}", name));

    string = env;
  }

  throw std::runtime_error(fmt::format("Unknown substition type {:?}", string));
}

void expandValueSubstitution(config_json &value) {
  if (not value.is_string())
    return;

  auto &string = value.get_ref<std::string &>();
  if (not string.starts_with("$"sv))
    return;

  if (string.starts_with("$$"sv)) {
    string.erase(0);
    return;
  }

  constexpr static auto JSON_PREFIX = "$json:"sv;
  if (string.starts_with(JSON_PREFIX)) {
    auto name = string.c_str() + JSON_PREFIX.length();
    auto env = std::getenv(name);
    if (not env)
      throw std::runtime_error(
          fmt::format("Failed to substitute environment variable {:?}", name));

    value = config_json::parse(env);
    return;
  }

  constexpr static auto STRING_PREFIX = "$string:"sv;
  if (string.starts_with(STRING_PREFIX)) {
    auto name = string.c_str() + STRING_PREFIX.length();
    auto env = std::getenv(name);
    if (not env)
      throw std::runtime_error(fmt::format(
          "Failed to substitute unknown environment variable {:?}", name));

    string = env;
    return;
  }

  throw std::runtime_error(fmt::format("Unknown substition type {:?}", string));
}

config_json parseLibconfigSetting(::config_setting_t const *setting,
                                  config_json::options_t opts) {
  auto logger = Log::get("config");

  switch (config_setting_type(setting)) {
  case CONFIG_TYPE_ARRAY:
  case CONFIG_TYPE_LIST: {
    auto array = villas::config_json::array();
    for (auto const idx : std::views::iota(0, config_setting_length(setting))) {
      auto const elem = config_setting_get_elem(setting, idx);
      array.push_back(parseLibconfigSetting(elem, opts));
    }

    return array;
  }

  case CONFIG_TYPE_GROUP: {
    auto object = villas::config_json::object();
    for (auto const idx : std::views::iota(0, config_setting_length(setting))) {
      auto const elem = config_setting_get_elem(setting, idx);
      auto name = std::string(config_setting_name(elem));
      if (opts.expand_substitutions)
        expandStringSubstitution(name);
      object[std::move(name)] = parseLibconfigSetting(elem, opts);
    }

    return object;
  }

  case CONFIG_TYPE_STRING: {
    auto json = config_json(std::string(config_setting_get_string(setting)));
    if (not deprecatedSubstitution(json, opts) and opts.expand_substitutions)
      expandValueSubstitution(json);
    return json;
  }

  case CONFIG_TYPE_INT: {
    return config_setting_get_int(setting);
  }

  case CONFIG_TYPE_INT64: {
    return config_setting_get_int64(setting);
  }

  case CONFIG_TYPE_FLOAT: {
    return config_setting_get_float(setting);
  }

  case CONFIG_TYPE_BOOL: {
    return config_setting_get_bool(setting);
  }

  case CONFIG_TYPE_NONE:
  default: {
    return nullptr;
  }
  }
}

extern "C" char const **libconfigIncludeFunc(::config_t *config, char const *,
                                             char const *pattern,
                                             char const **error) noexcept {
  auto opts = static_cast<config_json::options_t *>(config_get_hook(config));
  auto paths = std::vector<fs::path>{};

  if (not opts->expand_includes) {
    *error = "include directives are disabled";
    return nullptr;
  }

  auto pattern_json = config_json(pattern);
  deprecatedSubstitution(pattern_json,
                         {
                             .expand_substitutions = opts->expand_substitutions,
                             .expand_deprecated = opts->expand_deprecated,
                         });
  auto const &pattern_expanded = pattern_json.get_ref<std::string const &>();

  try {
    paths = utils::glob(pattern_expanded, opts->include_directories);
    std::erase_if(paths, [](auto const &path) {
      auto discardErrorCode = std::error_code{};
      return not fs::is_regular_file(path, discardErrorCode);
    });
  } catch (...) {
  }

  if (paths.empty()) {
    *error = "include directive did not match any file";
    return nullptr;
  }

  auto ret =
      static_cast<char const **>(std::calloc(paths.size() + 1, sizeof(char *)));
  auto index = std::size_t{0};
  for (auto &path : paths)
    ret[index++] = strdup(path.c_str());

  return ret;
}

config_json readLibconfigFile(std::FILE *file, config_json::options_t opts) {
  using ConfigDeleter = decltype([](::config_t *c) { ::config_destroy(c); });
  using ConfigPtr = std::unique_ptr<::config_t, ConfigDeleter>;

  ::config_t config;
  ::config_init(&config);
  auto guard = ConfigPtr(&config);

  ::config_set_hook(&config, &opts);
  ::config_set_include_func(&config, &libconfigIncludeFunc);
  if (auto ret = ::config_read(&config, file); ret != CONFIG_TRUE) {
    throw std::runtime_error(fmt::format("Failed to load libconfig file: {}",
                                         config_error_text(&config)));
  }

  return parseLibconfigSetting(config_root_setting(&config), opts);
}

class ConfigParser {
public:
  ConfigParser(config_json::options_t const &opts = {}) : options(opts) {}

  bool operator()(int depth, config_json::parse_event_t event,
                  config_json &parsed) {
    switch (event) {
    case config_json::parse_event_t::key: {
      return parseKey(parsed);
    }

    case config_json::parse_event_t::value: {
      return parseValue(depth, parsed);
    }

    case config_json::parse_event_t::object_end: {
      return finishObject(depth, parsed);
    }

    default:
      return true;
    }
  }

private:
  struct Include {
    int depth;
    config_json value;
  };

  constexpr static auto INCLUDE_KEYWORD = "$include"sv;

  bool parseKey(config_json &key) {
    auto &keyString = key.get_ref<config_json::string_t &>();

    valueIsIncludePattern = (keyString == INCLUDE_KEYWORD);
    if (not valueIsIncludePattern and options.expand_substitutions)
      expandStringSubstitution(keyString);

    return true;
  }

  bool parseValue(int depth, config_json &value) {
    // TODO: remove deprecated substitions
    if (not deprecatedSubstitution(value, options) and
        options.expand_substitutions)
      expandValueSubstitution(value);

    if (not valueIsIncludePattern) {
      return true;
    }

    auto &pattern = value.get_ref<config_json::string_t &>();
    if (not options.expand_includes)
      throw std::runtime_error{fmt::format(
          "Failed to include {}: include directives are disabled", pattern)};

    for (auto path : utils::glob(pattern, options.include_directories)) {
      [[maybe_unused]] auto discardErrorCode = std::error_code{};
      if (not fs::is_regular_file(path, discardErrorCode))
        continue;

      auto includedValue = config_json::load_config_file(path, options);
      includes.push_back({depth, includedValue});
    }

    return false;
  }

  bool finishObject(int depth, config_json &object) {
    if (options.expand_includes) {
      while (not includes.empty() and includes.back().depth == depth + 1) {
        object.update(includes.back().value, true);
        includes.pop_back();
      }

      object.erase(INCLUDE_KEYWORD);
    }

    return true;
  }

  bool valueIsIncludePattern = false;
  std::vector<Include> includes = {};
  config_json::options_t options;
};

} // namespace

config_json config_json_base::load_config(std::FILE *f, options_t opts) {
  return config_json::parse(f, ConfigParser{opts});
}

config_json config_json_base::load_config(std::string_view s, options_t opts) {
  return config_json::parse(s.begin(), s.end(), ConfigParser{opts});
}

config_json config_json_base::load_config_file(fs::path const &path,
                                               options_t opts) {
  auto file = std::fopen(path.c_str(), "r");
  if (not file) {
    throw std::runtime_error(
        fmt::format("Failed to open config file {}", path.string()));
  }

  if (auto ext = path.extension(); ext == ".json") {
    return config_json::load_config(file, opts);
  } else if (ext == ".conf") {
    return readLibconfigFile(file, opts);
  } else {
    throw std::runtime_error(fmt::format(
        "Failed to load config file with unknown extension {}", ext.string()));
  }
}

} // namespace villas
