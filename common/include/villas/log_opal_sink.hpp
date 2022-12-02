/** Log sink for OPAL-RTs OpalPrint().
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

#include <mutex>

#include <villas/log.hpp>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>

namespace villas {
namespace node {

template<typename Mutex>
class OpalSink : public spdlog::sinks::base_sink<Mutex>
{

protected:
	void sink_it_(const spdlog::details::log_msg &msg) override
	{
#ifdef ENABLE_OPAL_ASYNC
		fmt::memory_buffer formatted;

		sink::formatter_->format(msg, formatted);

		auto str = fmt::to_string(formatted).c_str();

		OpalPrint(PROJECT_NAME ": %s\n", str);
#endif
	}

	void flush_() override
	{
		// Nothing to do
	}
};

using OpalSink_mt = OpalSink<std::mutex>;
using OpalSink_st = OpalSink<spdlog::details::null_mutex>;


} // namespace node
} // namespace villas
