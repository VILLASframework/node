/** Helper functions for sockets.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/* Define RTLAB before including OpalPrint.h for messages to be sent
 * to the OpalDisplay. Otherwise stdout will be used. */
#define RTLAB
#include "OpalPrint.h"
#include "AsyncApi.h"

#include "config.h"
#include "socket.h"

int socket_init(struct socket *s, Opal_GenAsyncParam_Ctrl IconCtrlStruct)
{
	struct ip_mreq mreq; /* Multicast group structure */
	unsigned char TTL = 1, LOOP = 0;
	int rc, proto, ret;

	proto = (int) IconCtrlStruct.FloatParam[0];
	if (proto != UDP_PROTOCOL) {
		OpalPrint("%s: This version of %s only supports UDP\n", PROGNAME, PROGNAME);
		return EIO;
	}
		
	
	OpalPrint("%s: Version        : %s\n", PROGNAME, VERSION);
	OpalPrint("%s: Remote Address : %s\n", PROGNAME, IconCtrlStruct.StringParam[0]);
	OpalPrint("%s: Remote Port    : %d\n", PROGNAME, (int) IconCtrlStruct.FloatParam[1]);

	/* Initialize the socket */
	s->sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s->sd < 0) {
		OpalPrint("%s: ERROR: Could not open socket\n", PROGNAME);
		return EIO;
	}

	/* Set the structure for the remote port and address */
	memset(&s->send_ad, 0, sizeof(s->send_ad));
	s->send_ad.sin_family = AF_INET;
	s->send_ad.sin_addr.s_addr = inet_addr(IconCtrlStruct.StringParam[0]);
	s->send_ad.sin_port = htons((u_short) IconCtrlStruct.FloatParam[1]);

	/* Set the structure for the local port and address */
	memset(&s->recv_ad, 0, sizeof(s->recv_ad));
	s->recv_ad.sin_family = AF_INET;
	s->recv_ad.sin_addr.s_addr = INADDR_ANY;
	s->recv_ad.sin_port = htons((u_short) IconCtrlStruct.FloatParam[2]);

	/* Bind local port and address to socket. */
	ret = bind(s->sd, (struct sockaddr *) &s->recv_ad, sizeof(struct sockaddr_in));
	if (ret == -1) {
		OpalPrint("%s: ERROR: Could not bind local port to socket\n", PROGNAME);
		return EIO;
	}
	else
		OpalPrint("%s: Local Port     : %d\n", PROGNAME, (int) IconCtrlStruct.FloatParam[2]);

	/* If sending to a multicast address */
	if ((inet_addr(IconCtrlStruct.StringParam[0]) & inet_addr("240.0.0.0")) == inet_addr("224.0.0.0")) {
		ret = setsockopt(s->sd, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &TTL, sizeof(TTL));
		if (ret == -1) {
			OpalPrint("%s: ERROR: Could not set TTL for multicast send (%d)\n", PROGNAME, errno);
			return EIO;
		}
		
		ret = setsockopt(s->sd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&LOOP, sizeof(LOOP));
		if (ret == -1) {
			OpalPrint("%s: ERROR: Could not set loopback for multicast send (%d)\n", PROGNAME, errno);
			return EIO;
		}

		OpalPrint("%s: Configured socket for sending to multicast address\n", PROGNAME);
	}

	/* If receiving from a multicast group, register for it. */
	if (inet_addr(IconCtrlStruct.StringParam[1]) > 0) {
		if ((inet_addr(IconCtrlStruct.StringParam[1]) & inet_addr("240.0.0.0")) == inet_addr("224.0.0.0")) {
			mreq.imr_multiaddr.s_addr = inet_addr(IconCtrlStruct.StringParam[1]);
			mreq.imr_interface.s_addr = INADDR_ANY;

			/* Have the multicast socket join the multicast group */
			ret = setsockopt(s->sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof(mreq));
			if (ret == -1) {
				OpalPrint("%s: ERROR: Could not join multicast group (%d)\n", PROGNAME, errno);
				return EIO;
			}

			OpalPrint("%s: Added process to multicast group (%s)\n",
				PROGNAME, IconCtrlStruct.StringParam[1]);
		}
		else {
			OpalPrint("%s: WARNING: IP address for multicast group is not in multicast range. Ignored\n",
				PROGNAME);
		}
	}

	return EOK;
}

int socket_close(struct socket *s, Opal_GenAsyncParam_Ctrl IconCtrlStruct)
{
	if (s->sd < 0) {
		shutdown(s->sd, SHUT_RDWR);
		close(s->sd);
	}

	return 0;
}

int socket_send(struct socket *s, char *data, int len)
{
	if (s->sd < 0)
		return -1;

	/* Send the packet */
	return sendto(s->sd, data, len, 0, (struct sockaddr *) &s->send_ad, sizeof(s->send_ad));
}

int socket_recv(struct socket *s, char *data, int len, double timeout)
{
	int ret;
	struct sockaddr_in client_ad;
	struct timeval tv;
	socklen_t client_ad_size = sizeof(client_ad);
	fd_set sd_set;

	if (s->sd < 0)
		return -1;

	/* Set the descriptor set for the select() call */
	FD_ZERO(&sd_set);
	FD_SET(s->sd, &sd_set);

	/* Set the tv structure to the correct timeout value */
	tv.tv_sec = (int) timeout;
	tv.tv_usec = (int) ((timeout - tv.tv_sec) * 1000000);

	/* Wait for a packet. We use select() to have a timeout. This is
	 * necessary when reseting the model so we don't wait indefinitely
	 * and prevent the process from exiting and freeing the port for
	 * a future instance (model load). */
	ret = select(s->sd + 1, &sd_set, (fd_set *) 0, (fd_set *) 0, &tv);
	switch (ret) {
		case -1: /* Error */
			return -1;
		case  0: /* We hit the timeout */
			return 0;
		default:
			if (!(FD_ISSET(s->sd, &sd_set))) {
				/* We received something, but it's not on "sd". Since sd is the only
				 * descriptor in the set... */
				OpalPrint("%s: RecvPacket: God, is that You trying to reach me?\n", PROGNAME);
				return -1;
			}
	}

	/* Clear the data array (in case we receive an incomplete packet) */
	memset(data, 0, len);

	/* Perform the reception */
	return recvfrom(s->sd, data, len, 0, (struct sockaddr *) &client_ad, &client_ad_size);
}