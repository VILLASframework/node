/** Signal metadata lits.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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

#include <list>
#include <memory>

#include <jansson.h>

#include <villas/log.hpp>
#include <villas/signal.hpp>

namespace villas {
namespace node {

class SignalList : public std::vector<Signal::Ptr> {

public:
	using Ptr = std::shared_ptr<SignalList>;

	SignalList()
	{ }

	SignalList(unsigned len, enum SignalType fmt);
	SignalList(const char *dt);

	int parse(json_t *json);

	Ptr clone();

	void dump(villas::Logger logger, const union SignalData *data = nullptr, unsigned len = 0) const;

	json_t * toJson() const;

	int getIndexByName(const std::string &name);
	Signal::Ptr getByName(const std::string &name);
	Signal::Ptr getByIndex(unsigned idx);
};

} /* namespace node */
} /* namespace villas */
