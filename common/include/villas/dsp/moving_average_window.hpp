/** A moving average window.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
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

} // namespace dsp
} // namespace villas
