/** A sliding/moving window.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <villas/utils.hpp>

namespace villas {
namespace dsp {

template<typename T>
class Window {

public:
	typedef typename std::vector<T>::size_type size_type;

protected:
	std::vector<T> data;

	T init;

	size_type steps;
	size_type mask;
	size_type pos;

public:
	Window(size_type s = 0, T i = 0) :
		init(i)
	{
		size_type len = LOG2_CEIL(s);

		/* Allocate memory for circular history buffer */
		data = std::vector<T>(len, i);

		steps = s;
		pos = len;
		mask = len - 1;
	}

	T update(T in)
	{
		T out = data[(pos - steps) & mask];

		data[pos++ & mask] = in;

		return out;
	}

	size_type getLength() const
	{
		return steps;
	}

	T operator[](int i) const
	{
		return data[(pos + i) & mask];
	}
};

} /* namespace dsp */
} /* namespace villas */
