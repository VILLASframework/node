/** Compile-time configuration.
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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#define PROGNAME	"VILLASnode-OPAL-UDP"
#define VERSION		"0.7"

#define MAX_VALUES	64

/* List of protocols */
#define VILLAS		1
#define GTNET_SKT	2

/* Default protocol */
#ifndef PROTOCOL
  #define PROTOCOL VILLAS
#endif /* PROTOCOL */

#endif /* _CONFIG_H_ */
