/** Wrapper around queue that uses POSIX CV's for signalling writes.
 *
 * @file
 * @author Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
	void push(const T &data)
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
