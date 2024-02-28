/** Log sink for OPAL-RTs OpalPrint().
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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
	void sink_it_(const spdlog::details::log_msg& msg) override
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
		/* nothing to do */
	}
};

using OpalSink_mt = OpalSink<std::mutex>;
using OpalSink_st = OpalSink<spdlog::details::null_mutex>;


} // namespace node
} // namespace villas
