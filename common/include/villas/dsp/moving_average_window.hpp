/** A moving average window.
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

#include <villas/dsp/window.hpp>

namespace villas {
namespace dsp {

template<typename T>
class MovingAverageWindow {

public:
	typedef typename Window<T>::size_type size_type;

protected:
	Window<T> window;

	T state;

public:
	MovingAverageWindow(size_type len, T i = 0) :
		window(len, i),
		state(i)
	{ }

	T update(T in)
	{
		T out = window.update(in);

		state += in;
		state -= out;

		return state / window.getSteps();
	}
};

} // namespace dsp
} // namespace villas
