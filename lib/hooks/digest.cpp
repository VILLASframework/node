/* Digest hook.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@opal-rt.com>
 * SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>

#include <fmt/core.h>
#include <fmt/format.h>
#include <openssl/evp.h>

#include <villas/exceptions.hpp>
#include <villas/hook.hpp>
#include <villas/sample.hpp>
#include <villas/signal_data.hpp>
#include <villas/signal_type.hpp>
#include <villas/timing.hpp>
#include <villas/utils.hpp>

namespace villas {
namespace node {
namespace digest {

template <typename T>
static std::array<std::uint8_t, sizeof(T)>
little_endian_bytes(T const &t) noexcept {
  auto bytes = std::array<std::uint8_t, sizeof(T)>();
  auto const ptr = reinterpret_cast<uint8_t const *>(std::addressof(t));
  std::copy(ptr, ptr + sizeof(T), bytes.begin());

#if BYTE_ORDER == BIG_ENDIAN
  std::reverse(bytes.begin(), bytes.end());
#endif

  return bytes;
}

static void FILE_free(FILE *f) { fclose(f); }

class DigestHook : public Hook {
  using md_ctx_ptr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
  using file_ptr = std::unique_ptr<FILE, decltype(&FILE_free)>;

  // Parameters
  std::string algorithm;
  std::string uri;

  // Context
  md_ctx_ptr md_ctx;
  EVP_MD const *md;
  file_ptr file;
  std::optional<uint64_t> first_sequence;
  std::optional<timespec> first_timestamp;
  std::optional<uint64_t> last_sequence;
  std::optional<timespec> last_timestamp;
  std::vector<unsigned char> md_buffer;
  std::string md_string_buffer;

  void emitDigest() {
    if (!first_sequence || !first_timestamp || !last_sequence ||
        !last_timestamp)
      return;

    unsigned int md_size;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    if (!EVP_DigestFinal_ex(md_ctx.get(), md_value, &md_size))
      throw RuntimeError{"Could not finalize digest"};

    if (!EVP_MD_CTX_reset(md_ctx.get()))
      throw RuntimeError{"Could not reset digest context"};

    if (!EVP_DigestInit_ex(md_ctx.get(), md, NULL))
      throw RuntimeError{"Could not initialize digest"};

    md_string_buffer.clear();
    auto inserter = std::back_inserter(md_string_buffer);
    fmt::format_to(inserter, "{}.{:09}-{} ", first_timestamp->tv_sec,
                   first_timestamp->tv_nsec, *first_sequence);
    fmt::format_to(inserter, "{}.{:09}-{} ", last_timestamp->tv_sec,
                   last_timestamp->tv_nsec, *last_sequence);
    fmt::format_to(inserter, "{} ", algorithm);
    for (unsigned int i = 0; i < md_size; ++i)
      fmt::format_to(inserter, "{:02X}", md_value[i]);

    logger->debug("emit {}", md_string_buffer);
    md_string_buffer.push_back('\n');
    fputs(md_string_buffer.c_str(), file.get());
    fflush(file.get());
  }

  void updateInterval(Sample const *smp) {
    auto const next_sequence = smp->sequence;
    auto const next_timestamp = smp->ts.origin;

    if (smp->flags & (int)SampleFlags::NEW_FRAME) {
      emitDigest();
      first_sequence = next_sequence;
      first_timestamp = next_timestamp;
    }

    last_sequence = next_sequence;
    last_timestamp = next_timestamp;
  }

  void updateDigest(Sample const *smp) {
    md_buffer.clear();
    auto inserter = std::back_inserter(md_buffer);

    if (smp->flags & (int)SampleFlags::HAS_TS_ORIGIN) {
      auto const bytes_sec =
          little_endian_bytes<uint64_t>(smp->ts.origin.tv_sec);
      std::copy(std::cbegin(bytes_sec), std::cend(bytes_sec), inserter);
      auto const bytes_nsec =
          little_endian_bytes<uint64_t>(smp->ts.origin.tv_nsec);
      std::copy(std::cbegin(bytes_nsec), std::cend(bytes_nsec), inserter);
    }

    if (smp->flags & (int)SampleFlags::HAS_SEQUENCE) {
      auto const bytes = little_endian_bytes<uint64_t>(smp->sequence);
      std::copy(std::cbegin(bytes), std::cend(bytes), inserter);
    }

    if (smp->flags & (int)SampleFlags::HAS_DATA) {
      if (signals->size() < smp->length)
        throw RuntimeError{"Sample length longer than signal list."};

      for (unsigned int i = 0; i < smp->length; ++i) {
        auto const signal = signals->getByIndex(i);
        auto const &data = smp->data[i];

        switch (signal->type) {
        case SignalType::BOOLEAN: {
          auto const bytes = little_endian_bytes<uint8_t>(data.b ? 1 : 0);
          std::copy(std::cbegin(bytes), std::cend(bytes), inserter);
          break;
        }

        case SignalType::INTEGER: {
          auto const bytes = little_endian_bytes<uint64_t>(data.i);
          std::copy(std::cbegin(bytes), std::cend(bytes), inserter);
          break;
        }

        case SignalType::FLOAT: {
          auto const f = (data.f == data.f) ? data.f : std::nan("");
          auto const bytes = little_endian_bytes<double>(f);
          std::copy(std::cbegin(bytes), std::cend(bytes), inserter);
          break;
        }

        case SignalType::COMPLEX: {
          auto real = data.z.real();
          real = (real == real) ? real : std::nanf("");
          auto const rbytes = little_endian_bytes<float>(real);
          std::copy(std::cbegin(rbytes), std::cend(rbytes), inserter);

          auto imag = data.z.imag();
          imag = (imag == imag) ? imag : std::nanf("");
          auto const ibytes = little_endian_bytes<float>(imag);
          std::copy(std::cbegin(ibytes), std::cend(ibytes), inserter);
          break;
        }

        case SignalType::INVALID:
        default:
          throw RuntimeError{"Can calculate digest for invalid sample."};
        }
      }
    }

    if (!EVP_DigestUpdate(md_ctx.get(), md_buffer.data(), md_buffer.size()))
      throw RuntimeError{"Could not update digest"};
  }

public:
  DigestHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : Hook(p, n, fl, prio, en), algorithm(), uri(),
        md_ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free), md(nullptr),
        file(nullptr, &FILE_free), first_sequence(std::nullopt),
        first_timestamp(std::nullopt), last_sequence(std::nullopt),
        last_timestamp(std::nullopt), md_buffer(), md_string_buffer() {}

  void parse(json_t *json) override {
    Hook::parse(json);

    char const *uri_str;
    char const *mode_str;
    char const *algorithm_str;

    json_error_t err;
    int ret =
        json_unpack_ex(json, &err, 0, "{ s: s, s?: s, s?: s }", "uri", &uri_str,
                       "mode", &mode_str, "algorithm", &algorithm_str);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-digest");

    uri = std::string(uri_str);

    if (algorithm_str)
      algorithm = std::string(algorithm_str);
  }

  void prepare() override {
    Hook::prepare();

    file = file_ptr(fopen(uri.c_str(), "w"), &FILE_free);
    if (!file)
      throw RuntimeError{"Could not open file {}: {}", uri, strerror(errno)};

    md = EVP_get_digestbyname(algorithm.c_str());
    if (!md)
      throw RuntimeError{"Could not fetch algorithm {}", algorithm};
  }

  void start() override {
    Hook::start();

    if (!EVP_DigestInit_ex(md_ctx.get(), md, NULL))
      throw RuntimeError{"Could not initialize digest"};
  }

  Hook::Reason process(struct Sample *smp) override {
    assert(smp);
    assert(state == State::STARTED);

    updateInterval(smp);
    updateDigest(smp);

    return Reason::OK;
  }

  void stop() override {
    Hook::stop();

    first_sequence.reset();
    first_timestamp.reset();
    last_sequence.reset();
    last_timestamp.reset();
    if (!EVP_MD_CTX_reset(md_ctx.get()))
      throw RuntimeError{"Could not reset digest context"};
  }
};

// Register hook
static char n[] = "digest";
static char d[] = "Calculate the digest for a range of samples";
static HookPlugin<DigestHook, n, d, (int)Hook::Flags::PATH, 999> p;

} // namespace digest
} // namespace node
} // namespace villas
