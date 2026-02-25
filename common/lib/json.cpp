#include <cstring>
#include <regex>
#include <stdexcept>
#include <system_error>
#include <vector>

#include <fmt/format.h>
#include <fnmatch.h>
#include <libconfig.h>
#include <nlohmann/json.hpp>

#include <villas/jansson.hpp>
#include <villas/json.hpp>
#include <villas/log.hpp>
#include <villas/utils.hpp>

using namespace std::string_view_literals;

void to_json(villas::Json &json, ::json_t const *jansson) {
  switch (json_typeof(jansson)) {
    using enum json_type;

  case JSON_ARRAY: {
    std::size_t index;
    ::json_t const *value;

    json = villas::Json::array();
    json_array_foreach (jansson, index, value)
      json.push_back(value);
  } break;

  case JSON_OBJECT: {
    char const *key;
    std::size_t keylen;
    ::json_t const *value;

    json = villas::Json::object();
    // The const_cast below is safe as long as we don't replace the contained value
    // by accessing the underlying iterator with json_object_key_to_iter(key) and
    // json_object_iter_set or json_object_iter_set_new.
    //
    // There is no API in jansson value to iterate or even enumerate keys of a const object.
    //
    // See https://github.com/akheron/jansson/issues/578
    json_object_keylen_foreach (const_cast<::json_t *>(jansson), key, keylen,
                                value)
      json.emplace(std::string{key, keylen}, value);
  } break;

  case JSON_STRING: {
    json = std::string{json_string_value(jansson), json_string_length(jansson)};
  } break;

  case JSON_INTEGER: {
    json = json_integer_value(jansson);
  } break;

  case JSON_REAL: {
    json = json_real_value(jansson);
  } break;

  case JSON_TRUE: {
    json = true;
  } break;

  case JSON_FALSE: {
    json = false;
  } break;

  case JSON_NULL:
  default: {
    json = nullptr;
  } break;
  }
}

namespace villas {

void from_json(Json const &json, JanssonPtr &jansson) {
  switch (json.type()) {
    using value_t = Json::value_t;

  case value_t::array: {
    jansson.reset(json_array());
    for (auto const &item : json)
      json_array_append_new(jansson.get(), item.get<JanssonPtr>().release());
  } break;

  case value_t::object: {
    jansson.reset(json_object());
    for (auto const &[key, value] : json.items())
      json_object_setn_new_nocheck(jansson.get(), key.data(), key.size(),
                                   value.get<JanssonPtr>().release());
  } break;

  case value_t::string: {
    auto const string = json.get<Json::string_t const *>();
    jansson.reset(json_stringn_nocheck(string->data(), string->size()));
  } break;

  case value_t::number_integer: {
    auto const integer = json.get<Json::number_integer_t const *>();
    jansson.reset(json_integer(*integer));
  } break;

  case value_t::number_unsigned: {
    auto const integer = json.get<Json::number_unsigned_t const *>();
    jansson.reset(json_integer(*integer));
  } break;

  case value_t::number_float: {
    auto const real = json.get<Json::number_float_t const *>();
    jansson.reset(json_real(*real));
  } break;

  case value_t::boolean: {
    auto const boolean = json.get<Json::boolean_t const *>();
    jansson.reset(json_boolean(*boolean));
  } break;

  case value_t::null: {
    jansson.reset(json_null());
  } break;

  case value_t::binary:
    throw std::runtime_error{"cannot convert binary value to jansson"};

  case value_t::discarded:
    throw std::runtime_error{"cannot convert discarded value to jansson"};

  default:
    __builtin_unreachable();
  }
}

void to_json(Json &json, JanssonPtr const &jansson) {
  to_json(json, jansson.get());
}

namespace {

// implementation of deprecated variable substitutions
void expand_substitutions_deprecated(Json &value, bool resolve_env,
                                     fs::path const *include_dir) {
  if (not value.is_string())
    return;

  if (not resolve_env and not include_dir)
    return;

  constexpr static auto DEPRECATED_INCLUDE_KEYWORD = "@include "sv;
  auto logger = Log::get("config");
  auto string = value.get_ref<std::string &>();
  auto do_include = false;
  auto expanded = std::size_t{0};

  // check for legacy @include keyword
  if (include_dir and string.starts_with(DEPRECATED_INCLUDE_KEYWORD)) {
    do_include = true;
    expanded = DEPRECATED_INCLUDE_KEYWORD.length();
  }

  // legacy environment variable substitution syntax
  static auto const env_regex = std::regex(R"--(\$\{([^}]+)\})--");
  enum : std::size_t {
    CAPTURE_ALL = 0,
    CAPTURE_NAME,
  };

  // expand deprecated environment substition syntax
  std::smatch match;
  while (resolve_env and std::regex_search(string.cbegin() + expanded,
                                           string.cend(), match, env_regex)) {
    auto name = std::string(match[CAPTURE_NAME]);
    auto env_ptr = std::getenv(name.c_str());
    if (not env_ptr)
      throw std::runtime_error(
          fmt::format("Could substitute environment variable {:?}", name));

    auto env_value = std::string_view(env_ptr);
    auto [begin, end] = std::pair(match[CAPTURE_ALL]);
    string.replace(begin, end, env_value.begin(), env_value.end());
    expanded += match.position() + env_value.length();
  }

  // expand deprecated @include directive
  if (do_include) {
    auto pattern =
        std::string_view(string).substr(DEPRECATED_INCLUDE_KEYWORD.length());
    auto result = Json(nullptr);
    for (auto path : utils::glob(pattern, std::span(include_dir, 1))) {
      auto partial_result =
          load_config_deprecated(path, include_dir != nullptr, resolve_env);
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
}

Json parse_libconfig_setting(::config_setting_t const *setting,
                             bool resolve_env, fs::path const *include_dir) {
  auto logger = Log::get("config");

  switch (config_setting_type(setting)) {
  case CONFIG_TYPE_ARRAY:
  case CONFIG_TYPE_LIST: {
    auto array = Json::array();
    for (auto const idx : std::views::iota(0, config_setting_length(setting))) {
      auto const elem = config_setting_get_elem(setting, idx);
      array.push_back(parse_libconfig_setting(elem, resolve_env, include_dir));
    }

    return array;
  }

  case CONFIG_TYPE_GROUP: {
    auto object = Json::object();
    for (auto const idx : std::views::iota(0, config_setting_length(setting))) {
      auto const elem = config_setting_get_elem(setting, idx);
      auto name = std::string(config_setting_name(elem));
      object.emplace(std::move(name),
                     parse_libconfig_setting(elem, resolve_env, include_dir));
    }

    return object;
  }

  case CONFIG_TYPE_STRING: {
    auto json = Json(std::string(config_setting_get_string(setting)));
    expand_substitutions_deprecated(json, resolve_env, include_dir);
    return json;
  }

  case CONFIG_TYPE_INT: {
    return config_setting_get_int(setting);
  }

  case CONFIG_TYPE_INT64: {
    return std::int64_t{config_setting_get_int64(setting)};
  }

  case CONFIG_TYPE_FLOAT: {
    return config_setting_get_float(setting);
  }

  case CONFIG_TYPE_BOOL: {
    return static_cast<bool>(config_setting_get_bool(setting));
  }

  case CONFIG_TYPE_NONE:
  default: {
    return nullptr;
  }
  }
}

struct LibconfigHook {
  bool resolve_env;
  fs::path const *include_dir;
};

extern "C" char const **libconfig_include_func(::config_t *config, char const *,
                                               char const *pattern,
                                               char const **error) noexcept {
  auto hook = static_cast<LibconfigHook *>(config_get_hook(config));
  auto paths = std::vector<fs::path>{};

  if (not hook->include_dir) {
    *error = "include directives are disabled";
    return nullptr;
  }

  auto pattern_json = Json(pattern);
  expand_substitutions_deprecated(pattern_json, hook->resolve_env, nullptr);
  auto const &pattern_expanded = pattern_json.get_ref<std::string const &>();

  try {
    paths = utils::glob(pattern_expanded, std::span(hook->include_dir, 1));
    std::erase_if(paths, [](auto const &path) {
      auto ec = std::error_code{};
      return not fs::is_regular_file(path, ec);
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

Json load_libconfig_deprecated(std::FILE *file, bool resolve_env,
                               fs::path const *include_dir) {
  using ConfigDeleter = decltype([](::config_t *c) { ::config_destroy(c); });
  using ConfigPtr = std::unique_ptr<::config_t, ConfigDeleter>;

  auto hook = LibconfigHook{
      .resolve_env = resolve_env,
      .include_dir = include_dir,
  };

  ::config_t config;
  ::config_init(&config);
  auto guard = ConfigPtr(&config);

  ::config_set_hook(&config, &hook);
  ::config_set_include_func(&config, &libconfig_include_func);
  if (auto ret = ::config_read(&config, file); ret != CONFIG_TRUE) {
    throw std::runtime_error(fmt::format("Failed to load libconfig file: {}",
                                         config_error_text(&config)));
  }

  return parse_libconfig_setting(config_root_setting(&config), resolve_env,
                                 include_dir);
}

} // namespace

Json load_config_deprecated(fs::path const &path, bool resolve_inc,
                            bool resolve_env) {
  using FileDeleter = decltype([](std::FILE *c) { std::fclose(c); });
  using FilePtr = std::unique_ptr<std::FILE, FileDeleter>;

  auto file = FilePtr(std::fopen(path.c_str(), "r"));
  if (not file) {
    throw std::runtime_error(
        fmt::format("Failed to open config file {}", path.string()));
  }

  auto include_dir = path.parent_path();
  if (auto ext = path.extension(); ext == ".json") {
    auto parser_callback = [&](int depth, Json::parse_event_t event,
                               Json &value) {
      if (event == Json::parse_event_t::value)
        expand_substitutions_deprecated(value, resolve_env,
                                        resolve_inc ? &include_dir : nullptr);

      return true;
    };

    return Json::parse(file.get(), parser_callback);
  } else if (ext == ".conf") {
    return load_libconfig_deprecated(file.get(), resolve_env,
                                     resolve_inc ? &include_dir : nullptr);
  } else {
    throw std::runtime_error(fmt::format(
        "Failed to load config file with unknown extension {}", ext.string()));
  }
}

} // namespace villas
