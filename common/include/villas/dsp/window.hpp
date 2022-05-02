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

#include <ranges>
#include <deque>

namespace villas {
namespace dsp {

template<typename T, typename Container = std::deque<T>>
class Window : protected Container {

public:
	using iterator = typename Container::iterator;
	using size_type = typename Container::size_type;

protected:

	virtual
	T filter(T in, size_type i) const
	{
		return in;
	}

	class transform_iterator : public Container::const_iterator {
	protected:
		const Window *window;

		using base_iterator = typename Container::const_iterator;

	public:
		transform_iterator(const Window<T> *w) :
			base_iterator(w->Container::begin()),
			window(w)
		{ }

		T operator*()
		{
			auto i = (*this) - window->begin();
			const auto &v = base_iterator::operator*();

			return window->filter(v, i);
		}
	};

public:
	Window(size_type l = 0, T i = 0) :
		std::deque<T>(l, i)
	{ }

	T update(T in)
	{
		Container::push_back(in);

		auto out = (*this)[0];

		Container::pop_front();

		return out;
	}

	// Expose a limited number of functions from deque

	using Container::size;
	using Container::end;

	transform_iterator begin() const noexcept
	{
		return transform_iterator(this);
	}

	T operator[](size_type i) const noexcept
	{
		auto v = Container::operator[](i);

		return filter(v, i);
	}

	virtual
	T getCorrectionFactor() const
	{
		return 1.0;
	}
};

} /* namespace dsp */
} /* namespace villas */
