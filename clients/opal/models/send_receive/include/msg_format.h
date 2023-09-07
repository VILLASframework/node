/* Message format.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

// The current version number for the message format
#define MSG_VERSION 2

// TODO: Implement more message types
#define MSG_TYPE_DATA 0  // Message contains float values
#define MSG_TYPE_START 1 // Message marks the beginning of a new simulation case
#define MSG_TYPE_STOP 2  // Message marks the end of a simulation case

// The total size in bytes of a message
#define MSG_LEN(values) (sizeof(struct msg) + MSG_DATA_LEN(values))

// The length of \p values values in bytes
#define MSG_DATA_LEN(values) (sizeof(float) * (values))

// The offset to the first data value in a message
#define MSG_DATA_OFFSET(msg) ((char *)(msg) + offsetof(struct msg, data))

// Initialize a message with default values
#define MSG_INIT(len, seq)                                                     \
  (struct msg) {                                                               \
    .version = MSG_VERSION, .type = MSG_TYPE_DATA, .length = len,              \
    .sequence = seq, .id = 0                                                   \
  }

// The timestamp of a message in struct timespec format
#define MSG_TS(msg)                                                            \
  (struct timespec) { .tv_sec = (msg)->ts.sec, .tv_nsec = (msg)->ts.nsec }

// This message format is used by all clients
struct msg {
#if BYTE_ORDER == BIG_ENDIAN
  unsigned
      version : 4; // Specifies the format of the remaining message (see MGS_VERSION)
  unsigned type : 2;      // Data or control message (see MSG_TYPE_*)
  unsigned reserved1 : 2; // Reserved bits
#elif BYTE_ORDER == LITTLE_ENDIAN
  unsigned reserved1 : 2; // Reserved bits
  unsigned type : 2;      // Data or control message (see MSG_TYPE_*)
  unsigned
      version : 4; // Specifies the format of the remaining message (see MGS_VERSION)
#else
#error Invalid byte-order
#endif // BYTEORDER

  uint8_t id;      // An id which identifies the source of this sample
  uint16_t length; // The number of values in msg::data[]
  uint32_t
      sequence; // The sequence number is incremented by one for consecutive messages

  // A timestamp per message
  struct {
    uint32_t sec;  // Seconds since 1970-01-01 00:00:00
    uint32_t nsec; // Nanoseconds of the current second
  } ts;

  // The message payload
  union {
    float f;    // Floating point values
    uint32_t i; // Integer values
  } data[];
} __attribute__((packed));
