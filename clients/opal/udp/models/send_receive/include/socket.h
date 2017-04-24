/** Helper functions for sockets.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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
