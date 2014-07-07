/*-------------------------------------------------------------------
 *  OPAL-RT Technologies inc
 *
 *  Copyright (C) 2003. All rights reserved.
 *
 *  File name =         AsyncIP.c
 *  Last modified by =  Mathieu Dubé-Dallaire
 *
 *  Code example of an asynchronous program.  This program is started
 *  by the asynchronous controller and demonstrates how to send and 
 *  receive data to and from the asynchronous icons and a UDP or TCP
 *  port.
 *
 *  Feel free to use this as a starting point for your own asynchronous
 *  application. You should normally only have to modify the sections
 *  marked with: ****** Format to specific protocol here. ******.
 *
 *-----------------------------------------------------------------*/

#define PROGNAME "AsyncIP"

/* Standard ANSI C headers needed for this program */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#if defined(__QNXNTO__)
#       include <process.h>
#       include <sys/sched.h>
#       include <pthread.h>
#       include <devctl.h>
#       include <sys/dcmd_chr.h>
#elif defined(__linux__)
#       define _GNU_SOURCE   1
#       include <sched.h>
#	if defined(__redhawk__)
#		include <cpuset.h>
#		include <mpadvise.h>
#	endif
#endif

/* This is the message format */
#include "msg_types.h"

/* Define RTLAB before including OpalPrint.h for messages to be sent
 * to the OpalDisplay. Otherwise stdout will be used. */
#define RTLAB
#include "OpalPrint.h"
#include "AsyncApi.h"
#include "AsyncIPUtils.h"

/* This is just for initializing the shared memory access to communicate
 * with the RT-LAB model. It's easier to remember the arguments like this */
#define	ASYNC_SHMEM_NAME argv[1]
#define ASYNC_SHMEM_SIZE atoi(argv[2])
#define PRINT_SHMEM_NAME argv[3]

/* This defines the maximum number of signals (doubles) that can be sent
 * or received by any individual Send or Recv block in the model. This
 * only applies to the "model <-> asynchronous process" communication. */
#define MAXSENDSIZE 64
#define MAXRECVSIZE 64

int AssignProcToCpu0(void)
{
#if defined(__linux__)
    #if defined(__redhawk__)
	int			rc;
	pid_t			pid = getpid();
	cpuset_t		*pCpuset;

	pCpuset = cpuset_alloc();
	if (NULL == pCpuset) {
		fprintf(stderr, "Error allocating a cpuset\n");
		return(ENOMEM);
	}

	cpuset_init(pCpuset);
	cpuset_set_cpu(pCpuset, 0, 1);

	rc = mpadvise(MPA_PRC_SETBIAS, MPA_TID, pid, pCpuset);
	if (MPA_FAILURE == rc) {
		rc = errno;
		fprintf(stderr, "Error from mpadvise, %d %s, for pid %d\n", errno, strerror(errno), pid);
		cpuset_free(pCpuset);
		return(rc);
	}

	cpuset_free(pCpuset);
	return EOK;
    #else
	cpu_set_t bindSet;
	CPU_ZERO(&bindSet);
	CPU_SET(0, &bindSet);

	/* changing process cpu affinity */
	if (sched_setaffinity(0, sizeof(cpu_set_t), &bindSet) != 0) {
		fprintf(stderr, "Unable to bind the process to CPU 0. (sched_setaffinity errno %d)\n", errno );
		return EINVAL;
	}
	return EOK;
    #endif
#endif	/* __linux__ */
}
void *SendToIPPort(void * arg)
{
	int SendID = 1;
	int i, n;
	int nbSend = 0;
	int ModelState;

	double mdldata[MAXSENDSIZE];
	int mdldata_size;
	struct data_out comdata;
	int comdata_size;
	int count = 0;

	OpalPrint("%s: SendToIPPort thread started\n", PROGNAME);

	OpalGetNbAsyncSendIcon(&nbSend);

	if (nbSend >= 1) {
		do {
			/* This call unblocks when the 'Data Ready' line of a send icon is asserted. */
			if ((n = OpalWaitForAsyncSendRequest (&SendID)) != EOK) {
				ModelState = OpalGetAsyncModelState();
				if ((ModelState != STATE_RESET) && (ModelState != STATE_STOP)) {
					OpalSetAsyncSendIconError(n, SendID);
					OpalPrint("%s: OpalWaitForAsyncSendRequest(), errno %d\n", PROGNAME, n);
				}
				continue;
			}

			/* No errors encountered yet */
			OpalSetAsyncSendIconError(0, SendID);

			/* Get the size of the data being sent by the unblocking SendID */
			OpalGetAsyncSendIconDataLength (&mdldata_size, SendID);
			if (mdldata_size/sizeof(double) > MAXSENDSIZE) {
				OpalPrint("%s: Number of signals for SendID=%d exceeds allowed maximum (%d)\n",
					PROGNAME, SendID, MAXSENDSIZE);
				return NULL;
			}

			/* Read data from the model */
			OpalGetAsyncSendIconData (mdldata, mdldata_size, SendID);

/******* FORMAT TO SPECIFIC PROTOCOL HERE ******************************
 * Modify this section to use values differently or to cast them
 * to other data types depending on the structure...
 */
		comdata.dev_id  = SendID;  /* Use the SendID as a device ID here */
		comdata.msg_id++;          /* The message ID is just incremented */
		comdata.msg_len = mdldata_size;
//		comdata.msg_len = (mdldata_size/sizeof(double)) * sizeof(int); /* If comdata.data was an "int" */

/* In our case, because the data in the packet is in double format
 * we don't need to cast the data from the model to another format */
		for (i=0; i < (mdldata_size / sizeof(double)); i++)
			comdata.data[i] = mdldata[i];
//			comdata.data[i] = (int)mdldata[i]; /* If comdata.data was an "int" */

/* Because the useful length of the structure is variable, we use
 * the comdata_size variable to send just what is necessary with
 * the write function. */
		comdata_size = 8 + comdata.msg_len;
//        	comdata_size = sizeof(comdata);   /* For fixed length packets */
/**********************************************************************/

			/* Perform the actual write to the ip port */
			if (SendPacket((char*)&comdata, comdata_size) < 0)
				OpalSetAsyncSendIconError(errno, SendID);
			else
				OpalSetAsyncSendIconError(0, SendID);

			/* This next call allows the execution of the "asynchronous" process
			 * to actually be synchronous with the model. To achieve this, you
			 * should set the "Sending Mode" in the Async_Send block to
			 * NEED_REPLY_BEFORE_NEXT_SEND or NEED_REPLY_NOW. This will force
			 * the model to wait for this process to call this
			 * OpalAsyncSendRequestDone function before continuing. */
			OpalAsyncSendRequestDone(SendID);

			/* Before continuing, we make sure that the real-time model
			 * has not been stopped. If it has, we quit. */
			ModelState = OpalGetAsyncModelState();
		} while ((ModelState != STATE_RESET) && (ModelState != STATE_STOP));

		OpalPrint("%s: SendToIPPort: Finished\n", PROGNAME);
	}
	else {
		OpalPrint("%s: SendToIPPort: No transimission block for this controller. Stopping thread.\n", PROGNAME);
	}

	return NULL;
}

void *RecvFromIPPort (void * arg)
{
	int RecvID = 1;
	int i, n1, n2, nt, n;
	int nbRecv = 0;
	int ModelState;

	double mdldata[MAXRECVSIZE];
	int mdldata_size;
	struct data_in comdata;
	int comdata_size;

	OpalPrint("%s: RecvFromIPPort thread started\n", PROGNAME);

	OpalGetNbAsyncRecvIcon(&nbRecv);

	if (nbRecv >= 1) {
		do {
			memset (&comdata, 0, sizeof(comdata));

/******* FORMAT TO SPECIFIC PROTOCOL HERE ******************************
 * Modify this section if your protocol needs to receive more than one
 * packet to process the data */
			comdata_size = sizeof(comdata);
			n  = RecvPacket((char*)&comdata, comdata_size, 1.0);
			nt = n;

/* In our example protocol, the length of the message is variable and it
 * is specified in the header. If your protocol is fixed-length, you can
 * remove this line. */
			comdata_size = 8 + comdata.msg_len;
/***********************************************************************/

			if (n < 1) {
				ModelState = OpalGetAsyncModelState();
				if ((ModelState != STATE_RESET) && (ModelState != STATE_STOP)) {
					// n ==  0 means timeout, so we continue silently
					//if (n == 0)
					//	OpalPrint("%s: Timeout while waiting for data\n", PROGNAME, errno);
					// n == -1 means a more serious error, so we print it
					if (n == -1)
						OpalPrint("%s: Error %d while waiting for data\n", PROGNAME, errno);
					continue;
				}
				break;
			}
			else if (nt != comdata_size) {
				/* Disable this print. It may happen in TCP/IP mode.
				 * The server needs to be modified to check packet size. */
				// OpalPrint("%s: Received incoherent packet (size: %d, complete: %d)\n", PROGNAME, nt, comdata_size);
				continue;
			}

/******* FORMAT TO SPECIFIC PROTOCOL HERE ******************************
 * Modify this section to use values differently or to cast them
 * to other data types depending on the structure...  */
			RecvID = comdata.dev_id;				/* Use the deviceID as the RecvID */
			OpalSetAsyncRecvIconStatus(comdata.msg_id, RecvID);	/* Set the Status to the message ID */
			OpalSetAsyncRecvIconError(0, RecvID);			/* Set the Error to 0 */

			/* Get the number of signals to send back to the model */
			OpalGetAsyncRecvIconDataLength(&mdldata_size, RecvID);

			if (mdldata_size/sizeof(double) > MAXRECVSIZE) {
				OpalPrint("%s: Number of signals for RecvID=%d (%d) exceeds allowed maximum (%d)\n",
					PROGNAME, RecvID, mdldata_size/sizeof(double), MAXRECVSIZE);
				return NULL;
			}

//			if (mdldata_size/sizeof(double) > comdata.msg_len/sizeof(int)) // If comdata.data was an "int"
			if (mdldata_size > comdata.msg_len) {
				OpalPrint("%s: Number of signals for RecvID=%d (%d) exceeds what was received (%d)\n",
					PROGNAME, RecvID, mdldata_size/sizeof(double), comdata.msg_len/sizeof(double));
			}

			for (i=0; i < (mdldata_size / sizeof(double)); i++)
				mdldata[i] = (double)comdata.data[i];
/************************************************************************/

			OpalSetAsyncRecvIconData(mdldata, mdldata_size, RecvID);

			/* Before continuing, we make sure that the real-time model
			 * has not been stopped. If it has, we quit. */
			ModelState = OpalGetAsyncModelState();
		} while ((ModelState != STATE_RESET) && (ModelState != STATE_STOP));

		OpalPrint("%s: RecvFromIPPort: Finished\n", PROGNAME);
	}
	else {
		OpalPrint("%s: RecvFromIPPort: No reception block for this controller. Stopping thread.\n", PROGNAME);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	Opal_GenAsyncParam_Ctrl IconCtrlStruct;
	int err;
	pthread_t tid_send,  tid_recv;
	pthread_attr_t attr_send, attr_recv;

	/* Check for the proper arguments to the program */
	if (argc < 4) {
		printf("Invalid Arguments: 1-AsyncShmemName 2-AsyncShmemSize 3-PrintShmemName\n");
		exit(0);
	}

	/* Enable the OpalPrint function. This prints to the OpalDisplay. */
	if (OpalSystemCtrl_Register(PRINT_SHMEM_NAME) != EOK) {
		printf("%s: ERROR: OpalPrint() access not available\n", PROGNAME);
		exit(EXIT_FAILURE);
	}

	/* Open Share Memory created by the model. */
	if ((OpalOpenAsyncMem(ASYNC_SHMEM_SIZE, ASYNC_SHMEM_NAME)) != EOK) {
		OpalPrint("%s: ERROR: Model shared memory not available\n", PROGNAME);
		exit(EXIT_FAILURE);
	}

	/* For Redhawk, Assign this process to CPU 0 in order to support partial XHP */
	AssignProcToCpu0();

	/* Get IP Controler Parameters (ie: ip address, port number...) and
	 * initialize the device on the QNX node. */
	memset(&IconCtrlStruct, 0, sizeof(IconCtrlStruct));
	if ((err = OpalGetAsyncCtrlParameters(&IconCtrlStruct, sizeof(IconCtrlStruct))) != EOK) {
		OpalPrint("%s: ERROR: Could not get controller parameters (%d).\n", PROGNAME, err);
		exit(EXIT_FAILURE);
	}

	if (InitSocket(IconCtrlStruct) != EOK) {
		OpalPrint("%s: ERROR: Initialization failed.\n", PROGNAME);
		exit(EXIT_FAILURE);
	}

	/* Start send/receive threads */
	if ((pthread_create(&tid_send, NULL, SendToIPPort, NULL)) == -1)
		OpalPrint("%s: ERROR: Could not create thread (SendToIPPort), errno %d\n", PROGNAME, errno);
	if ((pthread_create(&tid_recv, NULL, RecvFromIPPort, NULL)) == -1)
		OpalPrint("%s: ERROR: Could not create thread (RecvFromIPPort), errno %d\n", PROGNAME, errno);

	/* Wait for both threads to finish */
	if ((err = pthread_join(tid_send, NULL)) != 0)
		OpalPrint("%s: ERROR: pthread_join (SendToIPPort), errno %d\n", PROGNAME, err);
	if ((err = pthread_join(tid_recv, NULL)) != 0)
		OpalPrint("%s: ERROR: pthread_join (RecvFromIPPort), errno %d\n", PROGNAME, err);

	/* Close the ip port and shared memories */
	CloseSocket(IconCtrlStruct);
	OpalCloseAsyncMem (ASYNC_SHMEM_SIZE, ASYNC_SHMEM_NAME);
	OpalSystemCtrl_UnRegister(PRINT_SHMEM_NAME);

	return 0;
}
