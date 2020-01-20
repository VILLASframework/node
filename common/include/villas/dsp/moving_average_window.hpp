/** A moving average window.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstddef>
#include <villas/dsp/window.hpp>

namespace villas {
namespace dsp {

template<typename T>
class MovingAverageWindow : public Window<T> {

protected:
	T state;

public:
	MovingAverageWindow(size_t len, T i = 0) :
		Window<T>(len, i),
		state(i)
	{ }

	T update(T in)
	{
		T out = Window<T>::update(in);

		state += in;
		state -= out;

		return state / (double) Window<T>::getLength();
	}
};

} /* namespace dsp */
} /* namespace villas */
