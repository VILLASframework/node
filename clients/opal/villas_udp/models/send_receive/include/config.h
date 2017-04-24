/** Compile-time configuration.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/
 
#ifndef _CONFIG_H_
#define _CONFIG_H_

#define PROGNAME	"VILLASnode-OPAL-UDP"
#define VERSION		"0.6"

#define MAX_VALUES	64

/* List of protocols */
#define VILLAS		1
#define GTNET_SKT	2

/* Default protocol */
#ifndef PROTOCOL
  #define PROTOCOL VILLAS
#endif

#endif /* _CONFIG_H_ */