/* Read / write sample data in different formats.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <villas/list.hpp>
#include <villas/plugin.hpp>
#include <villas/sample.hpp>
#include <villas/signal_list.hpp>

namespace villas {
namespace node {

// Forward declarations
class FormatFactory;

class Format {

  friend FormatFactory;

public:
  using Ptr = std::unique_ptr<Format>;

protected:
  int flags;          // A set of flags which is automatically used.
  int real_precision; // Number of digits used for floatint point numbers

  Logger logger;

  struct {
    char *buffer;
    size_t buflen;
  } in, out;

  SignalList::Ptr
      signals; // Signal meta data for parsed samples by Format::scan()

public:
  Format(int fl);

  virtual ~Format();

  const SignalList::Ptr getSignals() const { return signals; }

  int getFlags() const { return flags; }

  void start(const SignalList::Ptr sigs, int fl = (int)SampleFlags::ALL);
  void start(const std::string &dtypes, int fl = (int)SampleFlags::ALL);

  virtual void start() {}

  virtual void parse(json_t *json);

  virtual int print(FILE *f, const struct Sample *const smps[], unsigned cnt);

  virtual int scan(FILE *f, struct Sample *const smps[], unsigned cnt);

  /* Print \p cnt samples from \p smps into buffer \p buf of length \p len.
	 *
	 * @param buf[out]	The buffer which should be filled with serialized data.
	 * @param len[in]	The length of the buffer \p buf.
	 * @param rbytes[out]	The number of bytes which have been written to \p buf. Ignored if nullptr.
	 * @param smps[in]	The array of pointers to samples.
	 * @param cnt[in]	The number of pointers in the array \p smps.
	 *
	 * @retval >=0		The number of samples from \p smps which have been written into \p buf.
	 * @retval <0		Something went wrong.
	 */
  virtual int sprint(char *buf, size_t len, size_t *wbytes,
                     const struct Sample *const smps[], unsigned cnt) = 0;

  /* Parse samples from the buffer \p buf with a length of \p len bytes.
	 *
	 * @param buf[in]	The buffer of data which should be parsed / de-serialized.
	 * @param len[in]	The length of the buffer \p buf.
	 * @param rbytes[out]	The number of bytes which have been read from \p buf.
	 * @param smps[out]	The array of pointers to samples.
	 * @param cnt[in]	The number of pointers in the array \p smps.
	 *
	 * @retval >=0		The number of samples which have been parsed from \p buf and written into \p smps.
	 * @retval <0		Something went wrong.
	 */
  virtual int sscan(const char *buf, size_t len, size_t *rbytes,
                    struct Sample *const smps[], unsigned cnt) = 0;

  // Wrappers for sending a (un)parsing single samples

  int print(FILE *f, const struct Sample *smp) { return print(f, &smp, 1); }

  int scan(FILE *f, struct Sample *smp) { return scan(f, &smp, 1); }

  int sprint(char *buf, size_t len, size_t *wbytes, const struct Sample *smp) {
    return sprint(buf, len, wbytes, &smp, 1);
  }

  int sscan(const char *buf, size_t len, size_t *rbytes, struct Sample *smp) {
    return sscan(buf, len, rbytes, &smp, 1);
  }
};

class BinaryFormat : public Format {

public:
  using Format::Format;
};

class FormatFactory : public plugin::Plugin {

public:
  using plugin::Plugin::Plugin;

  virtual Format *make() = 0;

  static Format *make(json_t *json);

  static Format *make(const std::string &format);

  virtual void init(Format *f) { f->logger = getLogger(); }

  virtual std::string getType() const { return "format"; }

  virtual bool isHidden() const { return false; }
};

template <typename T, const char *name, const char *desc, int flags = 0>
class FormatPlugin : public FormatFactory {

public:
  using FormatFactory::FormatFactory;

  virtual Format *make() {
    auto *f = new T(flags);

    init(f);

    return f;
  }

  virtual std::string getName() const { return name; }

  virtual std::string getDescription() const { return desc; }
};

} // namespace node
} // namespace villas
