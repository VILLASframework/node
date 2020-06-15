/** Wrapper around queue that uses POSIX CV's for signalling writes.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
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

#include <mutex>
#include <queue>

namespace villas {

template<typename T>
class Queue {

protected:
	std::queue<T> queue;
	std::mutex mtx;

public:
	void push(T p)
	{
		std::unique_lock<std::mutex> guard(mtx);

		queue.push(p);
	}

	T pop()
	{
		std::unique_lock<std::mutex> guard(mtx);

		T res = queue.front();
		queue.pop();

		return res;
	}

	bool empty()
	{
		std::unique_lock<std::mutex> guard(mtx);

		return queue.empty();
	}

};

} /* namespace villas */
