/** Wrapper around queue that uses POSIX CV's for signalling writes.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
