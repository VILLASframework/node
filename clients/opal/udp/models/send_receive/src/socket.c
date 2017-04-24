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
#include <netinet/in.h>
#include <arpa/inet.h>

/* Define RTLAB before including OpalPrint.h for messages to be sent
 * to the OpalDisplay. Otherwise stdout will be used. */
#define RTLAB
#include "OpalPrint.h"
#include "AsyncApi.h"

#include "config.h"
#include "socket.h"

/* Globals variables */
struct sockaddr_in send_ad;	/* Send address */
struct sockaddr_in recv_ad;	/* Receive address */
int sd = -1;			/* socket descriptor */

int InitSocket(Opal_GenAsyncParam_Ctrl IconCtrlStruct)
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
	sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sd < 0) {
		OpalPrint("%s: ERROR: Could not open socket\n", PROGNAME);
		return EIO;
	}

	/* Set the structure for the remote port and address */
	memset(&send_ad, 0, sizeof(send_ad));
	send_ad.sin_family = AF_INET;
	send_ad.sin_addr.s_addr = inet_addr(IconCtrlStruct.StringParam[0]);
	send_ad.sin_port = htons((u_short)IconCtrlStruct.FloatParam[1]);

	/* Set the structure for the local port and address */
	memset(&recv_ad, 0, sizeof(recv_ad));
	recv_ad.sin_family = AF_INET;
	recv_ad.sin_addr.s_addr = INADDR_ANY;
	recv_ad.sin_port = htons((u_short)IconCtrlStruct.FloatParam[2]);

	/* Bind local port and address to socket. */
	ret = bind(sd, (struct sockaddr *) &recv_ad, sizeof(struct sockaddr_in));
	if (ret == -1) {
		OpalPrint("%s: ERROR: Could not bind local port to socket\n", PROGNAME);
		return EIO;
	}
	else
		OpalPrint("%s: Local Port     : %d\n", PROGNAME, (int) IconCtrlStruct.FloatParam[2]);

	/* If sending to a multicast address */
	if ((inet_addr(IconCtrlStruct.StringParam[0]) & inet_addr("240.0.0.0")) == inet_addr("224.0.0.0")) {
		ret = setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &TTL, sizeof(TTL));
		if (ret == -1) {
			OpalPrint("%s: ERROR: Could not set TTL for multicast send (%d)\n", PROGNAME, errno);
			return EIO;
		}
		
		ret = setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&LOOP, sizeof(LOOP));
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
			ret = setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof(mreq));
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

int CloseSocket(Opal_GenAsyncParam_Ctrl IconCtrlStruct)
{
	if (sd < 0) {
		shutdown(sd, SHUT_RDWR);
		close(sd);
	}

	return 0;
}

int SendPacket(char* DataSend, int datalength)
{
	if (sd < 0)
		return -1;

	/* Send the packet */
	return sendto(sd, DataSend, datalength, 0, (struct sockaddr *)&send_ad, sizeof(send_ad));
}

int RecvPacket(char* DataRecv, int datalength, double timeout)
{
	int len, ret;
	struct sockaddr_in client_ad;
	struct timeval tv;
	socklen_t client_ad_size = sizeof(client_ad);
	fd_set sd_set;

	if (sd < 0)
		return -1;

	/* Set the descriptor set for the select() call */
	FD_ZERO(&sd_set);
	FD_SET(sd, &sd_set);

	/* Set the tv structure to the correct timeout value */
	tv.tv_sec = (int) timeout;
	tv.tv_usec = (int) ((timeout - tv.tv_sec) * 1000000);

	/* Wait for a packet. We use select() to have a timeout. This is
	 * necessary when reseting the model so we don't wait indefinitely
	 * and prevent the process from exiting and freeing the port for
	 * a future instance (model load). */
	ret = select(sd+1, &sd_set, (fd_set *) 0, (fd_set *) 0, &tv);
	switch (ret) {
		case -1: /* Error */
			return -1;
		case  0: /* We hit the timeout */
			return 0;
		default:
			if (!(FD_ISSET(sd, &sd_set))) {
				/* We received something, but it's not on "sd". Since sd is the only
				 * descriptor in the set... */
				OpalPrint("%s: RecvPacket: God, is that You trying to reach me?\n", PROGNAME);
				return -1;
			}
	}

	/* Clear the DataRecv array (in case we receive an incomplete packet) */
	memset(DataRecv, 0, datalength);

	/* Perform the reception */
	return recvfrom(sd, DataRecv, datalength, 0, (struct sockaddr *) &client_ad, &client_ad_size);
}