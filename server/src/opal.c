/** Node type: OPAL (AsyncApi)
 *
 * This file implements the opal subtype for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include "opal.h"

int opal_print(struct node *n, char *buf, int len)
{

}

int opal_open(struct node *n)
{
	/* Enable the OpalPrint function. This prints to the OpalDisplay. */
	if (OpalSystemCtrl_Register(PRINT_SHMEM_NAME) != EOK) {
		printf("%s: ERROR: OpalPrint() access not available\n", PROGNAME);
		exit(EXIT_FAILURE);
	}

	OpalPrint("%s: This is a S2SS client\n", PROGNAME);

	/* Open Share Memory created by the model. */
	if ((OpalOpenAsyncMem(ASYNC_SHMEM_SIZE, ASYNC_SHMEM_NAME)) != EOK) {
		OpalPrint("%s: ERROR: Model shared memory not available\n", PROGNAME);
		exit(EXIT_FAILURE);
	}

}

int opal_close(struct node *n)
{
	OpalCloseAsyncMem (ASYNC_SHMEM_SIZE, ASYNC_SHMEM_NAME);
	OpalSystemCtrl_UnRegister(PRINT_SHMEM_NAME);
}

int opal_read(struct node *n, struct msg *m)
{
	/* This call unblocks when the 'Data Ready' line of a send icon is asserted. */
	if ((n = OpalWaitForAsyncSendRequest(&SendID)) != EOK) {
		ModelState = OpalGetAsyncModelState();
		if ((ModelState != STATE_RESET) && (ModelState != STATE_STOP)) {
			OpalSetAsyncSendIconError(n, SendID);
			OpalPrint("%s: OpalWaitForAsyncSendRequest(), errno %d\n", PROGNAME, n);
		}

		return -1; // FIXME: correct return value
	}

	/* No errors encountered yet */
	OpalSetAsyncSendIconError(0, SendID);

	/* Get the size of the data being sent by the unblocking SendID */
	OpalGetAsyncSendIconDataLength(&mdldata_size, SendID);
	if (mdldata_size / sizeof(double) > MSG_VALUES) {
		OpalPrint("%s: Number of signals for SendID=%d exceeds allowed maximum (%d)\n",
			PROGNAME, SendID, MSG_VALUES);

		return NULL;
	}

	/* Read data from the model */
	OpalGetAsyncSendIconData(mdldata, mdldata_size, SendID);

	msg.sequence = htons(seq++);
	msg.length = mdldata_size / sizeof(double);

	for (i = 0; i < msg.length; i++)
		msg.data[i].f = (float) mdldata[i];

	msg_size = MSG_LEN(msg.length);

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
	if ((ModelState == STATE_RESET) || (ModelState == STATE_STOP))
		return -1; // TODO: fixme

	return 0;
}

int opal_write(struct node *n, struct msg *m)
{

}
