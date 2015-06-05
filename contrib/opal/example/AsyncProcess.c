/*-------------------------------------------------------------------
 *  OPAL-RT Technologies inc
 *
 *  Copyright (C) 2003. All rights reserved.
 *
 *  File name =         AsyncSerial.c
 *  Last modified by =  Mathieu Dubé-Dallaire
 *
 *  Code example of an asynchronous program.  This program is started
 *  by the asynchronous controller and demonstrates how to send and 
 *  receive data to and from the asynchronous icons and a serial 
 *  COMM port (/dev/ser1, /dev/ser2).
 *
 *  Note: The Software Flow control is not fully functional in this 
 *        application.  The settings for the terminal device are 
 *        properly set but the 'RecvFromSerialPort' function does 
 *        not implement this functionality.
 *
 *  Feel free to use this as a starting point for your own asynchronous
 *  application. You should normally only have to modify the sections
 *  marked with: ****** Format to specific protocol here. ******.
 *
  *-----------------------------------------------------------------*/
#ifndef WIN32
#define PROGNAME "AsyncProcess"

// Standard ANSI C headers needed for this program
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>


#if defined(__QNXNTO__)
// For QNX v6.x threads
#	include <process.h>
#	include <sys/sched.h>
#	include <pthread.h>
#	include <devctl.h>
#	include <sys/dcmd_chr.h>
#elif defined(__linux__)
extern double fmin(double x, double y);
#endif
// Define RTLAB before including OpalPrint.h for messages to be sent
// to the OpalDisplay. Otherwise stdout will be used.
#define RTLAB
#include "OpalPrint.h"
#include "AsyncApi.h"

// This is just for initializing the shared memory access to communicate
// with the RT-LAB model. It's easier to remember the arguments like this
#define	ASYNC_SHMEM_NAME argv[1]
#define ASYNC_SHMEM_SIZE atoi(argv[2])
#define PRINT_SHMEM_NAME argv[3]

// This defines the maximum number of signals (doubles) that can be sent
// or received by any individual Send or Recv block in the model. This
// only applies to the "model <-> asynchronous process" communication.
#define MAXSENDSIZE 64
#define MAXRECVSIZE 64

// Set the stack size of each thread.
#define STACKSIZE 4096

volatile int thread_count = 0;
/************************************************************************/
void * ReceiveFromModel (void * arg)
{
  const int NB_SKIP = 499;
  int	 step = 0;
  unsigned int    SendID = 1;
  int    i,n;
  int    nbSend = 0, nbRecv = 0;
  int    ModelState;

  double mdldata[MAXSENDSIZE];
  int    mdldata_size;
  int    comdata_size;
  
  OpalPrint("%s: ReceiveFromModel thread started\n", PROGNAME);
  
  OpalGetNbAsyncSendIcon(&nbSend);
  OpalGetNbAsyncRecvIcon(&nbRecv);
  
  if(nbSend >= 1 && nbRecv >= 1)
  {
	do
	{
		// This call unblocks when the 'Data Ready' line of a send icon is asserted.
		if((n = OpalWaitForAsyncSendRequest (&SendID)) != 0)
	    {
			ModelState = OpalGetAsyncModelState();
			if ((ModelState != STATE_RESET) && (ModelState != STATE_STOP))
			{
				OpalSetAsyncSendIconError(n, SendID);
				OpalPrint("%s: OpalWaitForAsyncSendRequest(), errno %d\n", PROGNAME, n);
			}
			continue;
		}
	  
		// No errors encountered yet
		OpalSetAsyncSendIconError(0, SendID);

		// Get the size of the data being sent by the unblocking SendID
		OpalGetAsyncSendIconDataLength (&mdldata_size, SendID);
		if (mdldata_size/sizeof(double) > MAXSENDSIZE)
	    {
			OpalPrint("%s: Number of signals for SendID=%d exceeds allowed maximum (%d)\n", PROGNAME, SendID, MAXSENDSIZE);
			return NULL;
	    }

		// Read data from the model
		OpalGetAsyncSendIconData (mdldata, mdldata_size, SendID);

		if (step == 0)
		{
			// Reply to the model
			SendToModel(-mdldata[0]);
		}
		step = ++step % NB_SKIP;

		// This next call allows the execution of the "asynchronous" process
		// to actually be synchronous with the model. To achieve this, you
		// should set the "Sending Mode" in the Async_Send block to
		// NEED_REPLY_BEFORE_NEXT_SEND or NEED_REPLY_NOW. This will force
		// the model to wait for this process to call this
		// OpalAsyncSendRequestDone function before continuing.
		OpalAsyncSendRequestDone (SendID);

		// Before continuing, we make sure that the real-time model
		// has not been stopped. If it has, we quit.
		ModelState = OpalGetAsyncModelState();
	} while ((ModelState != STATE_RESET) && (ModelState != STATE_STOP) && (ModelState != STATE_ERROR));

	OpalPrint("%s: ReceiveFromModel: Finished\n", PROGNAME);
  }
  
  else
  {
    OpalPrint("%s: ReceiveFromModel: No transimission block for this controller. Stopping thread.\n", PROGNAME);
  }

  thread_count--;
  return NULL;
}
/************************************************************************/

/************************************************************************/
int SendToModel (double value)
{
	int    RecvID = 1;	
	double mdldata[MAXRECVSIZE];
	int    mdldata_size;
  
	// Get the number of signals to send back to the model
	OpalGetAsyncRecvIconDataLength (&mdldata_size, RecvID);
	
	if (mdldata_size/sizeof(double) > MAXRECVSIZE)
	{
		OpalPrint("%s: Number of signals for RecvID=%d (%d) exceeds allowed maximum (%d)\n", PROGNAME, RecvID, mdldata_size/sizeof(double), MAXRECVSIZE);
		return 7;
	}
	
	mdldata[0] = value;
		
	OpalSetAsyncRecvIconData   (mdldata, mdldata_size, RecvID);
	
	OpalSetAsyncRecvIconStatus (OP_ASYNC_DATA_READY, RecvID); // Set the Status to the message ID
	OpalSetAsyncRecvIconError  (0, RecvID);              // Set the Error to 0

	return 0;
}
/************************************************************************/

/************************************************************************/
int main (int argc, char *argv[]) 
{
  //Opal_GenAsyncParam_Ctrl IconCtrlStruct;
  int err;
  pthread_t      tid_send,  tid_recv;
  pthread_attr_t attr_send, attr_recv;
  
  // Check for the proper arguments to the program
  if (argc < 4)
  {
      printf("Invalid Arguments: 1-AsyncShmemName 2-AsyncShmemSize 3-PrintShmemName\n");
      exit(0);
  }
  
  // Enable the OpalPrint function. This prints to the OpalDisplay.
  if (OpalSystemCtrl_Register(PRINT_SHMEM_NAME) != 0)
  {
      printf("%s: ERROR: OpalPrint() access not available\n", PROGNAME);
      exit(-1);	
  }
  
  // Open Share Memory created by the model. -----------------------
  if((OpalOpenAsyncMem (ASYNC_SHMEM_SIZE, ASYNC_SHMEM_NAME)) != 0)
  {
      OpalPrint("%s: ERROR: Model shared memory not available\n", PROGNAME);
      exit(-1);
  }

  // Get Serial Controler Parameters
  /*
  memset(&IconCtrlStruct, 0, sizeof(IconCtrlStruct));
  if((err = OpalGetAsyncCtrlParameters(&IconCtrlStruct, sizeof(IconCtrlStruct))) != 0)
  {
      OpalPrint("%s: ERROR: Could not get controller parameters (%d).\n", PROGNAME, err);
      exit(-1);
  }
  OpalPrint("----------------------\n");
  OpalPrint("ControllerID: %d \n",	(int)IconCtrlStruct.controllerID);
  OpalPrint("----------------------\n");
  */

  // Start reception thread -----------------------------------------
  thread_count++;
  pthread_attr_init (&attr_recv);
  //pthread_attr_setstacksize (&attr_recv, STACKSIZE); // Has been known to crash the application
  if ((pthread_create (&tid_recv, &attr_recv, ReceiveFromModel, NULL)) == -1)
  {
      OpalPrint("%s: ERROR: Could not create thread (ReceiveFromModel), errno %d\n", PROGNAME, errno);
      thread_count--;
  }

  // Wait for thread to finish --------------------------------
  if ((err = pthread_join (tid_recv, NULL)) != 0)
  {
	  OpalPrint("%s: ERROR: pthread_join (ReceiveFromModel), errno %d\n", PROGNAME, err);
  }


  // Close the shared memories ----------------------
  OpalCloseAsyncMem (ASYNC_SHMEM_SIZE,ASYNC_SHMEM_NAME);
  OpalSystemCtrl_UnRegister(PRINT_SHMEM_NAME);

  return(0);
}
/************************************************************************/
#endif