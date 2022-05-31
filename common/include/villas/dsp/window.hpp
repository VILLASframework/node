/** A sliding/moving window.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

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
		Container(l, i)
	{ }

	T val(size_type pos)
	{
		return this->Container::operator[](pos);
	}

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
};

} /* namespace dsp */
} /* namespace villas */
