/** Helper functions for sockets.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <netinet/in.h>

#define RT
#include "OpalGenAsyncParamCtrl.h"

#define UDP_PROTOCOL	1
#define TCP_PROTOCOL	2

struct socket {
	struct sockaddr_in send_ad;	/* Send address */
	struct sockaddr_in recv_ad;	/* Receive address */
	int sd;				/* socket descriptor */
};

int socket_init(struct socket *s, Opal_GenAsyncParam_Ctrl IconCtrlStruct);

int socket_send(struct socket *s, char *data, int len);

int socket_recv(struct socket *s, char *data, int len, double timeout);

int socket_close(struct socket *s, Opal_GenAsyncParam_Ctrl IconCtrlStruct);

#endif /* _SOCKET_H_ */
