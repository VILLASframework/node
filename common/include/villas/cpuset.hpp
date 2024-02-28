/** Human readable cpusets.
 *
 * @file
 * @author Steffen Vogel <github@daniel-krebs.net>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#ifdef __linux__

#include <sched.h>
#include <cstdint>

#include <villas/exceptions.hpp>

namespace villas {
namespace utils {

class CpuSet {

protected:
	cpu_set_t *setp;

	unsigned num_cpus;
	size_t sz;

public:

	CpuSet() :
		num_cpus(sizeof(uintmax_t) * 8),
		sz(CPU_ALLOC_SIZE(num_cpus))
	{

		setp = CPU_ALLOC(num_cpus);
		if (!setp)
			throw villas::RuntimeError("Failed to allocated memory");

		zero();
	}

	/** Parses string with list of CPU ranges.
	 *
	 * @param str Human readable representation of the set.
	 */
	CpuSet(const std::string &str);

	CpuSet(const char *str);

	/** Convert integer to cpu_set_t.
	 *
	 * @param set An integer number which is used as the mask
	 */
	CpuSet(uintmax_t set);

	/** Convert cpu_set_t to an integer. */
	operator uintmax_t();

	operator const cpu_set_t*()
	{
		return setp;
	}

	/** Returns human readable representation of the cpuset.
	 *
	 * The output format is a list of CPUs with ranges (for example, "0,1,3-9").
	 */
	operator std::string();

	~CpuSet()
	{
		CPU_FREE(setp);
	}

	CpuSet(const CpuSet &src) :
		CpuSet(src.num_cpus)
	{
		memcpy(setp, src.setp, sz);
	}

	bool empty() const
	{
		return count() == 0;
	}

	bool full() const
	{
		return count() == num_cpus;
	}

	unsigned count() const
	{
		return CPU_COUNT_S(sz, setp);
	}

	void zero()
	{
		CPU_ZERO_S(sz, setp);
	}

	size_t size() const
	{
		return sz;
	}

	CpuSet operator~()
	{
		CpuSet full = UINTMAX_MAX;

		return full ^ *this;
	}

	bool operator==(const CpuSet &rhs)
	{
		return CPU_EQUAL_S(sz, setp, rhs.setp);
	}

	CpuSet& operator&=(const CpuSet &rhs)
	{
		CPU_AND_S(sz, setp, setp, rhs.setp);
		return *this;
	}

	CpuSet& operator|=(const CpuSet &rhs)
	{
		CPU_OR_S(sz, setp, setp, rhs.setp);
		return *this;
	}

	CpuSet& operator^=(const CpuSet &rhs)
	{
		CPU_XOR_S(sz, setp, setp, rhs.setp);
		return *this;
	}

	friend CpuSet operator&(CpuSet lhs, const CpuSet &rhs)
	{
		lhs &= rhs;
		return lhs;
	}

	friend CpuSet operator|(CpuSet lhs, const CpuSet &rhs)
	{
		lhs |= rhs;
		return lhs;
	}

	friend CpuSet operator^(CpuSet lhs, const CpuSet &rhs)
	{
		lhs ^= rhs;
		return lhs;
	}

	//bool& operator[](std::size_t cpu)
	//{
	//	void cpuset_set_cpu(cpuset_t*setp, cpu_t cpu, int state)
	//}

	bool operator[](size_t cpu) const
	{
		return isset(cpu);
	}

	bool isset(size_t cpu) const
	{
		return CPU_ISSET_S(cpu, sz, setp);
	}

	void clear(size_t cpu)
	{
		CPU_CLR_S(cpu, sz, setp);
	}

	void set(size_t cpu)
	{
		CPU_SET_S(cpu, sz, setp);
	}
};

} // namespace utils
} // namespace villas

#endif
