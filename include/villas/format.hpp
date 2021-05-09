/** Read / write sample data in different formats.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#pragma once

#include <villas/list.h>
#include <villas/plugin.hpp>
#include <villas/sample.h>

namespace villas {
namespace node {

class Format {

protected:

	int flags;		/**< A set of flags which is automatically used. */

	int real_precision;	/**< Number of digits used for floatint point numbers */

	bool destroy_signals;

	struct {
		char *buffer;
		size_t buflen;
	} in, out;

	struct vlist *signals;	/**< Signal meta data for parsed samples by Format::scan() */

public:
	Format(int fl);

	virtual bool isBinaryPayload()
	{ return false; }

	struct vlist * getSignals() const
	{ return signals; }

	int getFlags() const
	{ return flags; }

	virtual ~Format();

	void start(struct vlist *sigs, int fl = (int) SampleFlags::HAS_ALL);
	void start(const std::string &dtypes, int fl = (int) SampleFlags::HAS_ALL);

	virtual void start()
	{ }

	virtual void parse(json_t *json);

	virtual int print(FILE *f, const struct sample * const smps[], unsigned cnt);
	virtual int scan(FILE *f, struct sample * const smps[], unsigned cnt);

	/** Print \p cnt samples from \p smps into buffer \p buf of length \p len.
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
	virtual int sprint(char *buf, size_t len, size_t *wbytes, const struct sample * const smps[], unsigned cnt) = 0;

	/** Parse samples from the buffer \p buf with a length of \p len bytes.
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
	virtual int sscan(const char *buf, size_t len, size_t *rbytes, struct sample * const smps[], unsigned cnt) = 0;

	/* Wrappers for sending a (un)parsing single samples */

	int print(FILE *f, const struct sample *smp)
	{
		return print(f, &smp, 1);
	}

	int scan(FILE *f, struct sample *smp)
	{
		return scan(f, &smp, 1);
	}

	int sprint(char *buf, size_t len, size_t *wbytes, const struct sample *smp)
	{
		return sprint(buf, len, wbytes, &smp, 1);
	}

	int sscan(const char *buf, size_t len, size_t *rbytes, struct sample *smp)
	{
		return sscan(buf, len, rbytes, &smp, 1);
	}
};

class BinaryFormat : public Format {

public:
	using Format::Format;

	virtual bool isBinaryPayload()
	{ return true; }
};

class FormatFactory : public plugin::Plugin {

public:
	using plugin::Plugin::Plugin;

	virtual Format * make() = 0;

	static
	Format * make(json_t *json);

	static
	Format * make(const std::string &format);
};

template <typename T, const char *name, const char *desc, int flags = 0>
class FormatPlugin : public FormatFactory {

public:
	using FormatFactory::FormatFactory;

	virtual Format * make()
	{
		return new T(flags);
	}

	/// Get plugin name
	virtual std::string
	getName() const
	{ return name; }

	/// Get plugin description
	virtual std::string
	getDescription() const
	{ return desc; }
};

} /* namespace node */
} /* namespace villas */
