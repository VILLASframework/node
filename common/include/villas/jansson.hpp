#pragma once

#include <initializer_list>
#include <type_traits>
#include <vector>

#include <jansson.h>

#include <villas/exceptions.hpp>

namespace villas {

// smart pointer for ::json_t values.
//
// this class mirrors the interface of std::shared_ptr using the internal reference count of ::json_t
class JanssonPtr {
public:
  JanssonPtr() : inner(nullptr) {}
  explicit JanssonPtr(::json_t *json) : inner(json) {}

  JanssonPtr(JanssonPtr const &other) : inner(json_incref(other.inner)) {}
  JanssonPtr(JanssonPtr &&other) : inner(std::exchange(other.inner, nullptr)) {}
  ~JanssonPtr() { json_decref(inner); }

  JanssonPtr &operator=(JanssonPtr const &other) {
    json_decref(inner);
    inner = json_incref(other.inner);
    return *this;
  }

  JanssonPtr &operator=(JanssonPtr &&other) {
    json_decref(inner);
    inner = std::exchange(other.inner, nullptr);
    return *this;
  }

  ::json_t *release() { return std::exchange(inner, nullptr); }

  void reset() {
    json_decref(inner);
    inner = nullptr;
  }

  void reset(::json_t *json) {
    json_decref(inner);
    inner = json_incref(json);
  }

  void swap(JanssonPtr &other) { std::swap(inner, other.inner); }

  operator bool() { return inner != nullptr; }
  ::json_t *get() const { return inner; }
  ::json_t *operator->() const { return inner; }

  friend void swap(JanssonPtr &lhs, JanssonPtr &rhs) { lhs.swap(rhs); }
  friend auto operator<=>(JanssonPtr const &, JanssonPtr const &) = default;

private:
  ::json_t *inner;
};

// type trait helper for applying variadic default promotions
template <typename T> class va_default_promote {
  static auto impl() {
    using U = std::decay_t<T>;
    if constexpr (std::is_enum_v<U>) {
      if constexpr (std::is_convertible_v<U, std::underlying_type_t<U>>) {
        // unscoped enumerations are converted to their underlying type and then promoted using integer promotions
        return std::type_identity<
            decltype(+std::declval<std::underlying_type_t<U>>())>();
      } else {
        // scoped enumeration handling is implementation defined just pass them without promotions
        return std::type_identity<U>();
      }
    } else if constexpr (std::is_same_v<float, U>) {
      // float values are promoted to double
      return std::type_identity<double>();
    } else if constexpr (std::is_integral_v<U>) {
      // integral values are promoted using integer promotions
      return std::type_identity<decltype(+std::declval<U>())>();
    } else {
      // default case without any promotions
      return std::type_identity<U>();
    }
  }

public:
  using type = decltype(impl())::type;
};

template <typename T> using va_default_promote_t = va_default_promote<T>::type;

// helper type for validating format strings
enum class JanssonFormatArg {
  KEY,
  STRING,
  SIZE,
  INT,
  JSON_INT =
      std::is_same_v<json_int_t, int>
          ? std::underlying_type_t<JanssonFormatArg>(JanssonFormatArg::INT)
          : std::underlying_type_t<JanssonFormatArg>(JanssonFormatArg::INT) + 1,
  DOUBLE,
  JSON,
};

// make a JanssonFormatArg from a type using template specializations
template <typename T> consteval JanssonFormatArg makeJanssonFormatArg() {
  if (std::is_same_v<T, char const>)
    return JanssonFormatArg::KEY;

  if (std::is_same_v<T, char const *>)
    return JanssonFormatArg::STRING;

  if (std::is_same_v<T, size_t>)
    return JanssonFormatArg::SIZE;

  if (std::is_same_v<T, int> or std::is_same_v<T, unsigned int>)
    return JanssonFormatArg::INT;

  if (std::is_same_v<T, json_int_t> or
      std::is_same_v<T, std::make_unsigned_t<json_int_t>>)
    return JanssonFormatArg::JSON_INT;

  if (std::is_same_v<T, double>)
    return JanssonFormatArg::DOUBLE;

  if (std::is_same_v<T, json_t *>)
    return JanssonFormatArg::JSON;

  throw "Unsupported argument type";
}

template <typename T>
constexpr static JanssonFormatArg janssonFormatArg = makeJanssonFormatArg<T>();

// helper type for validating format strings with nested structures
enum class JanssonNestedStructure {
  OBJECT,
  ARRAY,
};

// compile time validator for json_unpack-style format strings
template <typename... Args> class JanssonUnpackFormatString {
public:
  consteval JanssonUnpackFormatString(char const *fmt) : fmt(fmt) {
#ifdef __cpp_lib_constexpr_vector
    validate(fmt);
#endif
  }

  constexpr char const *c_str() const { return fmt; }

private:
  consteval static void validate(char const *fmt) {
    constexpr auto ignored = std::string_view(" \t\n,:");

    auto const args =
        std::initializer_list{janssonFormatArg<std::remove_pointer_t<Args>>...};
    auto const string = std::string_view(fmt);
    auto const findNextToken = [&](std::size_t pos) {
      if (pos == std::string_view::npos)
        return std::string_view::npos;

      return string.find_first_not_of(ignored, pos);
    };

    std::size_t token = 0;
    auto arg = args.begin();
    auto nested = std::vector<JanssonNestedStructure>{};

    while ((token = findNextToken(token)) != std::string_view::npos) {
      if (not nested.empty() and
          nested.back() == JanssonNestedStructure::OBJECT) {
        if (string[token] == '}') {
          nested.pop_back();
          token++;
          continue;
        }

        if (string[token++] != 's')
          throw "Invalid specifier for object key";

        if (*arg++ != JanssonFormatArg::KEY)
          throw "Expected 'const char *' argument for object key";

        if ((token = findNextToken(token)) == std::string_view::npos)
          break;

        if (string[token] == '?')
          token++;

        if ((token = findNextToken(token)) == std::string_view::npos)
          break;
      }

      switch (string[token++]) {
      case 's': {
        if (*arg++ != JanssonFormatArg::STRING)
          throw "Expected 'const char **' argument for 's' specifier";

        if ((token = findNextToken(token)) == std::string_view::npos)
          break;

        if (string[token] == '%') {
          if (*arg++ != JanssonFormatArg::SIZE)
            throw "Expected 'size_t *' argument for 's%' specifier";
          token++;
        }
      } break;

      case 'n':
        break;

      case 'b': {
        if (*arg++ != JanssonFormatArg::INT)
          throw "Expected 'int *' argument for 'b' specifier";
      } break;

      case 'i': {
        if (*arg++ != JanssonFormatArg::INT)
          throw "Expected 'int *' argument for 'i' specifier";
      } break;

      case 'I': {
        if (*arg++ != JanssonFormatArg::JSON_INT)
          throw "Expected 'json_int_t *' argument for 'I' specifier";
      } break;

      case 'f': {
        if (*arg++ != JanssonFormatArg::DOUBLE)
          throw "Expected 'double *' argument for 'f' specifier";
      } break;

      case 'F': {
        if (*arg++ != JanssonFormatArg::DOUBLE)
          throw "Expected 'double *' argument for 'F' specifier";
      } break;

      case 'o': {
        if (*arg++ != JanssonFormatArg::JSON)
          throw "Expected 'json_t *' argument for 'o' specifier";
      } break;

      case 'O': {
        if (*arg++ != JanssonFormatArg::JSON)
          throw "Expected 'json_t *' argument for 'O' specifier";
      } break;

      case '[': {
        nested.push_back(JanssonNestedStructure::ARRAY);
      } break;

      case ']': {
        if (nested.empty() or nested.back() != JanssonNestedStructure::ARRAY)
          throw "Unexpected ]";
        nested.pop_back();
      } break;

      case '{': {
        nested.push_back(JanssonNestedStructure::OBJECT);
      } break;

      case '}':
        throw "Unexpected }";

      case '!':
        break;

      case '*':
        break;

      default:
        throw "Unknown format specifier in format string";
      }
    }

    if (not nested.empty())
      throw "Unclosed structure in format string";

    if (arg != args.end())
      throw "Unused trailing arguments";
  }

  char const *fmt;
};

template <typename... Args>
void janssonUnpack(::json_t *json,
                   JanssonUnpackFormatString<va_default_promote_t<Args>...> fmt,
                   Args... args) {
  ::json_error_t err;
  if (auto ret = ::json_unpack_ex(json, &err, 0, fmt.c_str(), args...);
      ret == -1)
    throw RuntimeError("Could not unpack json value: {}", err.text);
}

// compile time validator for json_pack-style format strings
template <typename... Args> class JanssonPackFormatString {
public:
  consteval JanssonPackFormatString(char const *fmt) : fmt(fmt) {
#ifdef __cpp_lib_constexpr_vector
    validate(fmt);
#endif
  }

  constexpr char const *c_str() const { return fmt; }

private:
  consteval static void validate(char const *fmt) {
    constexpr auto ignored = std::string_view(" \t\n,:");

    auto const args = std::initializer_list{janssonFormatArg<Args>...};
    auto const string = std::string_view(fmt);
    auto const findNextToken = [&](std::size_t pos) {
      if (pos == std::string_view::npos)
        return std::string_view::npos;

      return string.find_first_not_of(ignored, pos);
    };

    std::size_t token = 0;
    auto arg = args.begin();
    auto nested = std::vector<JanssonNestedStructure>{};
    while ((token = findNextToken(token)) != std::string_view::npos) {
      if (not nested.empty() and
          nested.back() == JanssonNestedStructure::OBJECT) {
        if (string[token] == '}') {
          nested.pop_back();
          token++;
          continue;
        }

        if (string[token++] != 's')
          throw "Invalid specifier for object key";

        if (*arg++ != JanssonFormatArg::STRING)
          throw "Expected 'const char *' argument for object key";

        if ((token = findNextToken(token)) == std::string_view::npos)
          break;

        if (string[token] == '#') {
          if (*arg++ != JanssonFormatArg::INT)
            throw "Expected 'int' argument for 's#' specifier";
          token++;
        } else if (string[token] == '%') {
          if (*arg++ != JanssonFormatArg::SIZE)
            throw "Expected 'size_t' argument for 's%' specifier";
          token++;
        }

        while ((token = findNextToken(token)) != std::string_view::npos) {
          if (string[token] != '+')
            break;

          if (*arg++ != JanssonFormatArg::STRING)
            throw "Expected 'const char *' argument for '+' specifier";

          if ((token = findNextToken(++token)) == std::string_view::npos)
            break;

          if (string[token] == '#') {
            if (*arg++ != JanssonFormatArg::INT)
              throw "Expected 'int' argument for '+#' specifier";
            token++;
          } else if (string[token] == '%') {
            if (*arg++ != JanssonFormatArg::SIZE)
              throw "Expected 'size_t' argument for 's%' specifier";
            token++;
          }
        }

        if (token == std::string_view::npos)
          throw "Expected closing }";
      }

      switch (string[token++]) {
      case 's': {
        if (*arg++ != JanssonFormatArg::STRING)
          throw "Expected 'const char *' argument for 's' specifier";

        if ((token = findNextToken(token)) == std::string_view::npos)
          break;

        auto optionalValue = false;
        if (string[token] == '?' or string[token] == '*') {
          optionalValue = true;
          token++;
        } else if (string[token] == '#') {
          if (*arg++ != JanssonFormatArg::INT)
            throw "Expected 'int' argument for 's#' specifier";
          token++;
        } else if (string[token] == '%') {
          if (*arg++ != JanssonFormatArg::SIZE)
            throw "Expected 'size_t' argument for 's%' specifier";
          token++;
        }

        if (optionalValue)
          break;

        while ((token = findNextToken(token)) != std::string_view::npos) {
          if (string[token] != '+')
            break;

          if (*arg++ != JanssonFormatArg::STRING)
            throw "Expected 'const char *' argument for '+' specifier";

          if ((token = findNextToken(++token)) == std::string_view::npos)
            break;

          if (string[token] == '#') {
            if (*arg++ != JanssonFormatArg::INT)
              throw "Expected 'int' argument for '+#' specifier";
            token++;
          } else if (string[token] == '%') {
            if (*arg++ != JanssonFormatArg::SIZE)
              throw "Expected 'size_t' argument for 's%' specifier";
            token++;
          }
        }
      } break;

      case '+':
        throw "Unexpected '+' specifier";

      case 'n':
        break;

      case 'b': {
        if (*arg++ != JanssonFormatArg::INT)
          throw "Expected 'int' argument for 'b' specifier";
      } break;

      case 'i': {
        if (*arg++ != JanssonFormatArg::INT)
          throw "Expected 'int' argument for 'i' specifier";
      } break;

      case 'I': {
        if (*arg++ != JanssonFormatArg::JSON_INT)
          throw "Expected 'json_int_t' argument for 'I' specifier";
      } break;

      case 'f': {
        if (*arg++ != JanssonFormatArg::DOUBLE)
          throw "Expected 'double' argument for 'f' specifier";
      } break;

      case 'o': {
        if (*arg++ != JanssonFormatArg::JSON)
          throw "Expected 'json_t *' argument for 'o' specifier";

        if (string[token] == '?' or string[token] == '*')
          token++;
      } break;

      case 'O': {
        if (*arg++ != JanssonFormatArg::JSON)
          throw "Expected 'json_t *' argument for 'O' specifier";

        if (string[token] == '?' or string[token] == '*')
          token++;
      } break;

      case '[': {
        nested.push_back(JanssonNestedStructure::ARRAY);
      } break;

      case ']': {
        if (nested.empty() or nested.back() != JanssonNestedStructure::ARRAY)
          throw "Unexpected ]";
        nested.pop_back();
      } break;

      case '{': {
        nested.push_back(JanssonNestedStructure::OBJECT);
      } break;

      case '}':
        throw "Unexpected }";

      default:
        throw "Unknown format specifier in format string";
      }
    }

    if (not nested.empty())
      throw "Unclosed structure in format string";

    if (arg != args.end())
      throw "Unused trailing arguments";
  }

  char const *fmt;
};

template <typename... Args>
JanssonPtr
janssonPack(JanssonPackFormatString<va_default_promote_t<Args>...> fmt,
            Args... args) {
  auto json = ::json_pack(fmt.c_str(), args...);
  if (json == nullptr)
    throw RuntimeError("Could not pack json value");

  return JanssonPtr(json);
}

} // namespace villas
