/** Signal meta data.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <memory>

#include <jansson.h>

#include <villas/signal_type.hpp>
#include <villas/signal_data.hpp>

/* "I" defined by complex.h collides with a define in OpenSSL */
#undef I

namespace villas {
namespace node {

/** Signal descriptor.
 *
 * This data structure contains meta data about samples values in struct Sample::data
 */
class Signal {

public:
	using Ptr = std::shared_ptr<Signal>;

	std::string name;		/**< The name of the signal. */
	std::string unit;		/**< The unit of the signal. */

	union SignalData init;		/**< The initial value of the signal. */
	enum SignalType type;

	/** Initialize a signal with default values. */
	Signal(const std::string &n = "", const std::string &u = "", enum SignalType t = SignalType::INVALID);

	/** Parse signal description. */
	int parse(json_t *json);

	std::string toString(const union SignalData *d = nullptr) const;

	/** Produce JSON representation of signal. */
	json_t * toJson() const;

	bool isNext(const Signal &sig);
};

} /* namespace node */
} /* namespace villas */
