/* Log sink for OPAL-RTs OpalPrint().
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <mutex>

#include <villas/log.hpp>
#include <villas/config.hpp>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>

extern "C" {
	/* Define RTLAB before including OpalPrint.h for messages to be sent
	* to the OpalDisplay. Otherwise stdout will be used. */
	#define RTLAB
	#include <OpalPrint.h>
}

namespace villas {
namespace node {

template<typename Mutex>
class OpalSink : public spdlog::sinks::base_sink<Mutex>
{

protected:
	void sink_it_(const spdlog::details::log_msg &msg) override
	{
#ifdef WITH_NODE_OPAL
		fmt::memory_buffer formatted;

		sink::formatter_->format(msg, formatted);

		auto str = fmt::to_string(formatted).c_str();

		OpalPrint(PROJECT_NAME ": %s\n", str);
#endif
	}

	void flush_() override
	{
		// nothing to do
	}
};

using OpalSink_mt = OpalSink<std::mutex>;
using OpalSink_st = OpalSink<spdlog::details::null_mutex>;

} // namespace node
} // namespace villas
