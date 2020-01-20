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
#include <condition_variable>

#include <villas/queue.hpp>

namespace villas {

template<typename T>
class QueueSignalled : public Queue<T> {

private:
	std::condition_variable cv;

public:
	void push(const T& data)
	{
		Queue<T>::push(data);

		cv.notify_one();
	}

	T pop()
	{
		std::unique_lock<std::mutex> l(Queue<T>::mtx);

		while (Queue<T>::queue.empty())
			cv.wait(l);

		T res = Queue<T>::queue.front();
		Queue<T>::queue.pop();

		return res;
	}
};

} // namespace villas
