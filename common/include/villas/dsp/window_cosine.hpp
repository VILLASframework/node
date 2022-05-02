/** A sliding/moving window.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <cmath>

#include <vector>

#include <villas/dsp/window.hpp>

namespace villas {
namespace dsp {



template<typename T>// a0 = 1.0, double a1 = 0.0, double a2 = 0.0, double a3 = 0.0, double a4 = 0.0>
class CosineWindow : public Window<T> {

public:
	using size_type = typename Window<T>::size_type;

protected:
	std::vector<T> coefficients;

	T correctionFactor;

	virtual
	T filter(T in, size_type i) const
	{
		return in * coefficients[i];
	}

public:
	CosineWindow(double a0, double a1, double a2, double a3, double a4, size_type len, T i = 0) :
		Window<T>(len, i),
		coefficients(len),
		correctionFactor(0)
	{
		for (unsigned i = 0; i < len; i++) {
			coefficients[i] = a0
					- a1 * cos(2 * M_PI * i / len)
					+ a2 * cos(4 * M_PI * i / len)
					- a3 * cos(6 * M_PI * i / len)
					+ a4 * cos(8 * M_PI * i / len);

			correctionFactor += coefficients[i];
		}

		correctionFactor /= len;
	}


	virtual
	T getCorrectionFactor() const
	{
		return correctionFactor;
	}
};

// From: https://en.wikipedia.org/wiki/Window_function#Cosine-sum_windows

template<typename T>
class HannWindow : public CosineWindow<T> {

public:
	HannWindow(typename Window<T>::size_type len, T i = 0) :
		CosineWindow<T>(0.5, 0.5, 0., 0., 0., len, i) {}
};

template<typename T>
class HammingWindow : public CosineWindow<T> {

public:
	HammingWindow(typename Window<T>::size_type len, T i = 0) :
		CosineWindow<T>(25./46, 1 - 25./46, 0., 0., 0., len, i) {}
};

template<typename T>
class FlattopWindow : public CosineWindow<T> {

public:
	FlattopWindow(typename Window<T>::size_type len, T i = 0) :
		CosineWindow<T>(0.21557895, 0.41663158, 0.277263158, 0.083578947, 0.006947368, len, i) {}
};

template<typename T>
class NuttallWindow : public CosineWindow<T> {

public:
	NuttallWindow(typename Window<T>::size_type len, T i = 0) :
		CosineWindow<T>(0.355768, 0.487396, 0.144232, 0.012604, 0., len, i) {}
};

template<typename T>
class BlackmanWindow : public CosineWindow<T> {

public:
	BlackmanWindow(typename Window<T>::size_type len, T i = 0) :
		CosineWindow<T>(0.3635819, 0.4891775, 0.1365995, 0.0106411, 0., len, i) {}
};

} /* namespace dsp */
} /* namespace villas */
