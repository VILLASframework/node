/** Helper functions for sockets.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#ifndef _SOCKET_H_
#define _SOCKET_H_

#define RT
#include "OpalGenAsyncParamCtrl.h"

#define UDP_PROTOCOL	1
#define TCP_PROTOCOL	2

int InitSocket(Opal_GenAsyncParam_Ctrl IconCtrlStruct);

int SendPacket(char* DataSend, int datalength);

int RecvPacket(char* DataRecv, int datalength, double timeout);

int CloseSocket(Opal_GenAsyncParam_Ctrl IconCtrlStruct);

#endif /* _SOCKET_H_ */
