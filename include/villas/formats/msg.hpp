/* Message related functions.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include<villas/signal_list.hpp>

namespace villas {

// Forward declarataions
struct List;

namespace node {

// Forward declarations
struct Message;
struct Sample;

// Convert msg from network to host byteorder
void msg_ntoh(struct Message *m);

// Convert msg from host to network byteorder
void msg_hton(struct Message *m);

// Convert msg header from network to host byteorder
void msg_hdr_hton(struct Message *m);

// Convert msg header from host to network byteorder
void msg_hdr_ntoh(struct Message *m);

/* Check the consistency of a message.
 *
 * The functions checks the header fields of a message.
 *
 * @param m A pointer to the message
 * @retval 0 The message header is valid.
 * @retval <0 The message header is invalid.
 */
int msg_verify(const struct Message *m);

// Copy fields from \p msg into \p smp.
int msg_to_sample(const struct Message *msg, struct Sample *smp, const SignalList::Ptr sigs, uint8_t *source_index);

// Copy fields form \p smp into \p msg.
int msg_from_sample(struct Message *msg, const struct Sample *smp, const SignalList::Ptr sigs, uint8_t source_index);

} // namespace node
} // namespace villas
