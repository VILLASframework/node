/* Utilities.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <list>
#include <string>
#include <vector>

#include <openssl/sha.h>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>

#include <villas/config.hpp>

#ifdef __GNUC__
#define LIKELY(x) __builtin_expect((x), 1)
#define UNLIKELY(x) __builtin_expect((x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

// Check assertion and exit if failed.
#ifndef assert
#define assert(exp)                                                            \
  do {                                                                         \
    if (!EXPECT(exp, 0))                                                       \
      error("Assertion failed: '%s' in %s(), %s:%d", XSTR(exp), __FUNCTION__,  \
            __BASE_FILE__, __LINE__);                                          \
  } while (0)
#endif

// CPP stringification
#define XSTR(x) STR(x)
#define STR(x) #x

#define CONCAT_DETAIL(x, y) x##y
#define CONCAT(x, y) CONCAT_DETAIL(x, y)
#define UNIQUE(x) CONCAT(x, __COUNTER__)

#ifdef ALIGN
#undef ALIGN
#endif
#define ALIGN(x, a) ALIGN_MASK(x, (uintptr_t)(a)-1)
#define ALIGN_MASK(x, m) (((uintptr_t)(x) + (m)) & ~(m))
#define IS_ALIGNED(x, a) (ALIGN(x, a) == (uintptr_t)x)

#define SWAP(x, y)                                                             \
  do {                                                                         \
    auto t = x;                                                                \
    x = y;                                                                     \
    y = t;                                                                     \
  } while (0)

// Round-up integer division
#define CEIL(x, y) (((x) + (y)-1) / (y))

// Get nearest up-rounded power of 2
#define LOG2_CEIL(x) (1 << (villas::utils::log2i((x)-1) + 1))

// Check if the number is a power of 2
#define IS_POW2(x) (((x) != 0) && !((x) & ((x)-1)))

// Calculate the number of elements in an array.
#define ARRAY_LEN(a) (sizeof(a) / sizeof(a)[0])

// Return the bigger value
#ifdef MAX
#undef MAX
#endif
#define MAX(a, b)                                                              \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a > _b ? _a : _b;                                                         \
  })

// Return the smaller value
#ifdef MIN
#undef MIN
#endif
#define MIN(a, b)                                                              \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a < _b ? _a : _b;                                                         \
  })
#define MIN3(a, b, c) MIN(MIN((a), (b)), (c))

#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

#ifndef container_of
#define container_of(ptr, type, member)                                        \
  ({                                                                           \
    const typeof(((type *)0)->member) *__mptr = (ptr);                         \
    (type *)((char *)__mptr - offsetof(type, member));                         \
  })
#endif

#define BITS_PER_LONGLONG (sizeof(long long) * 8)

// Some helper macros
#define BITMASK(h, l)                                                          \
  (((~0ULL) << (l)) & (~0ULL >> (BITS_PER_LONGLONG - 1 - (h))))
#define BIT(nr) (1UL << (nr))

namespace villas {
namespace utils {

std::vector<std::string> tokenize(const std::string &s,
                                  const std::string &delimiter);

template <typename T> void assertExcept(bool condition, const T &exception) {
  if (not condition)
    throw exception;
}

// Register a exit callback for program termination: SIGINT, SIGKILL & SIGALRM.
int signalsInit(void (*cb)(int signal, siginfo_t *sinfo, void *ctx),
                std::list<int> cbSignals = {},
                const std::list<int> &ignoreSignals = {SIGCHLD})
    __attribute__((warn_unused_result));

// Fill buffer with random data.
ssize_t readRandom(char *buf, size_t len);

// Remove ANSI control sequences for colored output.
char *decolor(char *str);

// Normal random variate generator using the Box-Muller method
//
// @param m Mean
// @param s Standard deviation
// @return Normal variate random variable (Gaussian)
double boxMuller(float m, float s);

// Double precission uniform random variable
double randf();

// Concat formatted string to an existing string.
//
// This function uses realloc() to resize the destination.
// Please make sure to only on dynamic allocated destionations!!!
//
// @param dest A pointer to a malloc() allocated memory region
// @param fmt A format string like for printf()
// @param ... Optional parameters like for printf()
// @retval The the new value of the dest buffer.
char *strcatf(char **dest, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

// Variadic version of strcatf()
char *vstrcatf(char **dest, const char *fmt, va_list &va)
    __attribute__((format(printf, 2, 0)));

char *strf(const char *fmt, ...);
char *vstrf(const char *fmt, va_list &va);

// Allocate and copy memory.
void *memdup(const void *src, size_t bytes);

// Call quit() in the main thread.
void die();

// Get log2 of long long integers
int log2i(long long x);

// Send signal \p sig to main thread.
void killme(int sig);

pid_t spawn(const char *name, char *const argv[]);

// Determines the string length as printed on the screen (ignores escable sequences).
size_t strlenp(const char *str);

// Calculate SHA1 hash of complete file \p f and place it into \p sha1.
//
// @param sha1[out] Must be SHA_DIGEST_LENGTH (20) in size.
// @retval 0 Everything was okay.
int sha1sum(FILE *f, unsigned char *sha1);

// Check if process is running inside a Docker container.
bool isDocker();

// Check if process is running inside a Kubernetes container.
bool isKubernetes();

// Check if process is running inside a containerized environment.
bool isContainer();

// Check if the process is running in a privileged environment (has SYS_ADMIN capability).
bool isPrivileged();

// helper type for std::visit
template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

// Explicit deduction guide (not needed as of C++20)
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void write_to_file(std::string data, const std::filesystem::path file);
std::vector<std::string>
read_names_in_directory(const std::filesystem::path &directory);

namespace base64 {

using byte = std::uint8_t;

std::string encode(const std::vector<byte> &input);
std::vector<byte> decode(const std::string &input);

} // namespace base64
} // namespace utils
} // namespace villas
