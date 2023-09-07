/* Compile-time configuration.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#define PROGNAME "VILLASnode-OPAL-UDP"
#define VERSION "0.7"

#define MAX_VALUES 64

// List of protocols
#define VILLAS 1
#define GTNET_SKT 2

// Default protocol
#ifndef PROTOCOL
#define PROTOCOL VILLAS
#endif // PROTOCOL

#endif // _CONFIG_H_
