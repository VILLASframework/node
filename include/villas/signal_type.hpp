/** Signal type.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <string>

namespace villas {
namespace node {

enum class SignalType {
	INVALID	= 0,	/**< Signal type is invalid. */
	FLOAT	= 1,	/**< See SignalData::f */
	INTEGER	= 2,	/**< See SignalData::i */
	BOOLEAN = 3,	/**< See SignalData::b */
	COMPLEX = 4	/**< See SignalData::z */
};

enum SignalType signalTypeFromString(const std::string &str);

enum SignalType signalTypeFromFormatString(char c);

std::string signalTypeToString(enum SignalType fmt);

enum SignalType signalTypeDetect(const std::string &val);

} /* namespace node */
} /* namespace villas */
