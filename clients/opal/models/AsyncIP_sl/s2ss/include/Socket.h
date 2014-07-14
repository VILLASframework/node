/** Helper functions for socket
 *
 *  Code example of an asynchronous program.  This program is started
 *  by the asynchronous controller and demonstrates how to send and 
 *  receive data to and from the asynchronous icons and a UDP or TCP
 *  port.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Mathieu Dubé-Dallaire
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @copyright 2003, OPAL-RT Technologies inc
 * @file
 */

#ifndef _SOCKET_H_
#define _SOCKET_H_

/* Modify this version line if you make changes to this file */
#define VERSION  "Opal-RT_20060524"

#define RT
#include "OpalGenAsyncParamCtrl.h"

#define UDP_PROTOCOL	1
#define TCP_PROTOCOL	2
#define	EOK		0

int InitSocket(Opal_GenAsyncParam_Ctrl IconCtrlStruct);

int SendPacket(char* DataSend, int datalength);

int RecvPacket(char* DataRecv, int datalength, double timeout);

int CloseSocket(Opal_GenAsyncParam_Ctrl IconCtrlStruct);

#endif /* _SOCKET_H_ */
