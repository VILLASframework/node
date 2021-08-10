/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include<villas/signal_list.hpp>

namespace villas {

/* Forward declarataions */
struct List;

namespace node {

/* Forward declaration */
struct Message;
struct Sample;

/** Convert msg from network to host byteorder */
void msg_ntoh(struct Message *m);

/** Convert msg from host to network byteorder */
void msg_hton(struct Message *m);

/** Convert msg header from network to host byteorder */
void msg_hdr_hton(struct Message *m);

/** Convert msg header from host to network byteorder */
void msg_hdr_ntoh(struct Message *m);

/** Check the consistency of a message.
 *
 * The functions checks the header fields of a message.
 *
 * @param m A pointer to the message
 * @retval 0 The message header is valid.
 * @retval <0 The message header is invalid.
 */
int msg_verify(const struct Message *m);

/** Copy fields from \p msg into \p smp. */
int msg_to_sample(const struct Message *msg, struct Sample *smp, const SignalList::Ptr sigs, uint8_t *source_index);

/** Copy fields form \p smp into \p msg. */
int msg_from_sample(struct Message *msg, const struct Sample *smp, const SignalList::Ptr sigs, uint8_t source_index);

} /* namespace node */
} /* namespace villas */
