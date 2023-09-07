/* Main routine of AsyncIP.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

// Standard ANSI C headers needed for this program
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Define RTLAB before including OpalPrint.h for messages to be sent
 * to the OpalDisplay. Otherwise stdout will be used. */
#define RTLAB
#include "AsyncApi.h"
#include "OpalPrint.h"

// This is the message format
#include "config.h"
#include "socket.h"
#include "utils.h"

#if PROTOCOL == VILLAS
#include "msg.h"
#include "msg_format.h"
#endif

/* This is just for initializing the shared memory access to communicate
 * with the RT-LAB model. It's easier to remember the arguments like this */
#define ASYNC_SHMEM_NAME argv[1]
#define ASYNC_SHMEM_SIZE atoi(argv[2])
#define PRINT_SHMEM_NAME argv[3]

// Global Variables
struct socket skt;

static void *SendToIPPort(void *arg) {
  unsigned int ModelState, SendID, Sequence = 0;
  int nbSend = 0, ret, cnt, len;

  // Data from OPAL-RT model
  double mdldata[MAX_VALUES];
  int mdldata_size;

#if PROTOCOL == VILLAS
  char buf[MSG_LEN(MAX_VALUES)];
  struct msg *msg = (struct msg *)buf;
#elif PROTOCOL == GTNET_SKT
  char buf[MAX_VALUES * sizeof(float)];
  float *msg = (float *)buf;
#endif

  OpalPrint("%s: SendToIPPort thread started\n", PROGNAME);

  OpalGetNbAsyncSendIcon(&nbSend);
  if (nbSend < 1) {
    OpalPrint("%s: SendToIPPort: No transimission block for this controller. "
              "Stopping thread.\n",
              PROGNAME);
    return NULL;
  }

  do {
    // This call unblocks when the 'Data Ready' line of a send icon is asserted.
    ret = OpalWaitForAsyncSendRequest(&SendID);
    if (ret != EOK) {
      ModelState = OpalGetAsyncModelState();
      if ((ModelState != STATE_RESET) && (ModelState != STATE_STOP)) {
        OpalSetAsyncSendIconError(ret, SendID);
        OpalPrint("%s: OpalWaitForAsyncSendRequest(), errno %d\n", PROGNAME,
                  ret);
      }

      continue;
    }

    // No errors encountered yet
    OpalSetAsyncSendIconError(0, SendID);

    // Get the size of the data being sent by the unblocking SendID
    OpalGetAsyncSendIconDataLength(&mdldata_size, SendID);
    cnt = mdldata_size / sizeof(double);
    if (cnt > MAX_VALUES) {
      OpalPrint("%s: Number of signals for SendID=%d exceeds allowed maximum "
                "(%d). Limiting...\n",
                PROGNAME, SendID, MAX_VALUES);
      cnt = MAX_VALUES;
    }

    // Read data from the model
    OpalGetAsyncSendIconData(mdldata, mdldata_size, SendID);

#if PROTOCOL == VILLAS
    // Get current time
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    msg->version = MSG_VERSION;
    msg->type = MSG_TYPE_DATA;
    msg->reserved1 = 0;
    msg->id = SendID;
    msg->length = cnt;
    msg->sequence = Sequence++;
    msg->ts.sec = now.tv_sec;
    msg->ts.nsec = now.tv_nsec;

    for (int i = 0; i < msg->length; i++)
      msg->data[i].f = (float)mdldata[i];

    len = MSG_LEN(msg->length);

    msg_hton(msg);
#elif PROTOCOL == GTNET_SKT
    uint32_t *imsg = (uint32_t *)msg;
    for (int i = 0; i < cnt; i++) {
      msg[i] = (float)mdldata[i];
      imsg[i] = htonl(imsg[i]);
    }

    len = cnt * sizeof(float);
#else
#error Unknown protocol
#endif

    // Perform the actual write to the ip port
    ret = socket_send(&skt, (char *)msg, len);
    if (ret < 0)
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

  return NULL;
}

static void *RecvFromIPPort(void *arg) {
  unsigned int ModelState, RecvID;
  int nbRecv = 0, ret, cnt;

  // Data from OPAL-RT model
  double mdldata[MAX_VALUES];
  int mdldata_size;

#if PROTOCOL == VILLAS
  char buf[MSG_LEN(MAX_VALUES)];
  struct msg *msg = (struct msg *)buf;
#elif PROTOCOL == GTNET_SKT
  char buf[MAX_VALUES * sizeof(float)];
  float *msg = (float *)buf;
#else
#error Unknown protocol
#endif

  OpalPrint("%s: RecvFromIPPort thread started\n", PROGNAME);

  OpalGetNbAsyncRecvIcon(&nbRecv);
  if (nbRecv < 1) {
    OpalPrint("%s: RecvFromIPPort: No reception block for this controller. "
              "Stopping thread.\n",
              PROGNAME);
    return NULL;
  }

  // Get list of RecvIds
  unsigned int RecvIDs[nbRecv];
  ret = OpalGetAsyncRecvIDList(RecvIDs, sizeof(RecvIDs));
  if (ret != EOK) {
    OpalPrint("%s: Failed to get list of RecvIDs\n", PROGNAME);
    return NULL;
  }

  do {
    // Receive message
    ret = socket_recv(&skt, (char *)msg, sizeof(buf), 1.0);
    if (ret < 1) {
      ModelState = OpalGetAsyncModelState();
      if ((ModelState != STATE_RESET) && (ModelState != STATE_STOP)) {
        if (ret == 0) // timeout, so we continue silently
          OpalPrint("%s: Timeout while waiting for data\n", PROGNAME, errno);
        if (ret == -1) // a more serious error, so we print it
          OpalPrint("%s: Error %d while waiting for data\n", PROGNAME, errno);

        continue;
      }
      break;
    }

#if PROTOCOL == VILLAS
    RecvID = msg->id;
#elif PROTOCOL == GTNET_SKT
    RecvID = 1;
#else
#error Unknown protocol
#endif
    // Check if this RecvID exists
    for (int i = 0; i < nbRecv; i++) {
      if (RecvIDs[i] == RecvID)
        goto found;
    }

    OpalPrint("%s: Received message with non-existent RecvID=%d. Changing to "
              "RecvID=%d...\n",
              PROGNAME, RecvID, RecvIDs[0]);
    RecvID = RecvIDs[0];

  found: // Get the number of signals to send back to the model
    OpalGetAsyncRecvIconDataLength(&mdldata_size, RecvID);
    cnt = mdldata_size / sizeof(double);
    if (cnt > MAX_VALUES) {
      OpalPrint("%s: Number of signals for RecvID=%d (%d) exceeds allowed "
                "maximum (%d)\n",
                PROGNAME, RecvID, cnt, MAX_VALUES);
      return NULL;
    }

#if PROTOCOL == VILLAS
    msg_ntoh(msg);

    ret = msg_verify(msg);
    if (ret) {
      OpalPrint("%s: Skipping invalid packet\n", PROGNAME);
      continue;
    }

    if (cnt > msg->length) {
      OpalPrint("%s: Number of signals for RecvID=%d (%d) exceeds what was "
                "received (%d)\n",
                PROGNAME, RecvID, cnt, msg->length);
    }

    for (int i = 0; i < msg->length; i++)
      mdldata[i] = (double)msg->data[i].f;

    // Update OPAL model
    OpalSetAsyncRecvIconStatus(msg->sequence,
                               RecvID); // Set the Status to the message ID
#elif PROTOCOL == GTNET_SKT
    uint32_t *imsg = (uint32_t *)msg;
    for (int i = 0; i < cnt; i++) {
      imsg[i] = ntohl(imsg[i]);
      mdldata[i] = (double)msg[i];
    }
#else
#error Unknown protocol
#endif

    OpalSetAsyncRecvIconError(0, RecvID); // Set the Error to 0

    OpalSetAsyncRecvIconData(mdldata, mdldata_size, RecvID);

    /* Before continuing, we make sure that the real-time model
		 * has not been stopped. If it has, we quit. */
    ModelState = OpalGetAsyncModelState();
  } while ((ModelState != STATE_RESET) && (ModelState != STATE_STOP));

  OpalPrint("%s: RecvFromIPPort: Finished\n", PROGNAME);

  return NULL;
}

int main(int argc, char *argv[]) {
  int ret;

  Opal_GenAsyncParam_Ctrl IconCtrlStruct;

  pthread_t tid_send, tid_recv;

  OpalPrint("%s: This is %s client version %s\n", PROGNAME, PROGNAME, VERSION);

  // Check for the proper arguments to the program
  if (argc < 4) {
    printf("Invalid Arguments: 1-AsyncShmemName 2-AsyncShmemSize "
           "3-PrintShmemName\n");
    exit(0);
  }

  // Enable the OpalPrint function. This prints to the OpalDisplay.
  ret = OpalSystemCtrl_Register(PRINT_SHMEM_NAME);
  if (ret != EOK) {
    printf("%s: ERROR: OpalPrint() access not available\n", PROGNAME);
    exit(EXIT_FAILURE);
  }

  // Open Share Memory created by the model.
  ret = OpalOpenAsyncMem(ASYNC_SHMEM_SIZE, ASYNC_SHMEM_NAME);
  if (ret != EOK) {
    OpalPrint("%s: ERROR: Model shared memory not available\n", PROGNAME);
    exit(EXIT_FAILURE);
  }

  AssignProcToCpu0();

  /* Get IP Controler Parameters (ie: ip address, port number...) and
	 * initialize the device on the QNX node. */
  memset(&IconCtrlStruct, 0, sizeof(IconCtrlStruct));

  ret = OpalGetAsyncCtrlParameters(&IconCtrlStruct, sizeof(IconCtrlStruct));
  if (ret != EOK) {
    OpalPrint("%s: ERROR: Could not get controller parameters (%d).\n",
              PROGNAME, ret);
    exit(EXIT_FAILURE);
  }

  // Initialize socket
  ret = socket_init(&skt, IconCtrlStruct);
  if (ret != EOK) {
    OpalPrint("%s: ERROR: Failed to create socket.\n", PROGNAME);
    exit(EXIT_FAILURE);
  }

  // Start send/receive threads
  ret = pthread_create(&tid_send, NULL, SendToIPPort, NULL);
  if (ret < 0)
    OpalPrint("%s: ERROR: Could not create thread (SendToIPPort), errno %d\n",
              PROGNAME, errno);

  ret = pthread_create(&tid_recv, NULL, RecvFromIPPort, NULL);
  if (ret < 0)
    OpalPrint("%s: ERROR: Could not create thread (RecvFromIPPort), errno %d\n",
              PROGNAME, errno);

  // Wait for both threads to finish
  ret = pthread_join(tid_send, NULL);
  if (ret)
    OpalPrint("%s: ERROR: pthread_join (SendToIPPort), errno %d\n", PROGNAME,
              ret);

  ret = pthread_join(tid_recv, NULL);
  if (ret)
    OpalPrint("%s: ERROR: pthread_join (RecvFromIPPort), errno %d\n", PROGNAME,
              ret);

  // Close the ip port and shared memories
  socket_close(&skt, IconCtrlStruct);

  OpalCloseAsyncMem(ASYNC_SHMEM_SIZE, ASYNC_SHMEM_NAME);
  OpalSystemCtrl_UnRegister(PRINT_SHMEM_NAME);

  return 0;
}
