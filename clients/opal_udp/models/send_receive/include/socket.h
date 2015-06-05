/** Helper functions for socket
 *
 *  Code example of an asynchronous program.  This program is started
 *  by the asynchronous controller and demonstrates how to send and 
 *  receive data to and from the asynchronous icons and a UDP or TCP
 *  port.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Mathieu Dubé-Dallaire
 * @copyright 2003, OPAL-RT Technologies inc
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _SOCKET_H_
#define _SOCKET_H_

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
