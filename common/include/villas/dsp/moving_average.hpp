/** A moving average filter.
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

#include <villas/window.hpp>

namespace villas {
namespace dsp {

template<typename T>
class MovingAverage {

public:
	typedef typename Window<T>::size_type size_type;

protected:
	Window<T> window;

	size_type length;

	T state;

public:
	MovingAverage(size_type len, T i) :
		window(len, i),
		length(len),
		state(i)
	{ }

	T update(T in)
	{
		T out = window.update(in);

		state += in;
		state -= out;

		return state / len;
	}
};

} // namespace dsp
} // namespace villas
