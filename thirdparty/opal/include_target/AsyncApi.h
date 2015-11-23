/*-------------------------------------------------------------------
 *	OPAL-RT Technologies inc
 *
 *	Copyright (C) 1999. All rights reserved.
 *
 *	File name =			$Workfile: AsyncApi.h $
 *	SourceSafe path =	$Logfile: /SIMUPAR/soft/common/include_target/AsyncApi.h $
 *	SourceSafe rev. =	$Revision: 2.14 $
 *	Last checked in =	$Date: 2009/06/04 01:39:35 $
 *	Last updated =		$Modtime: 8/14/02 3:43p $
 *	Last modified by =	$Author: irenep $
 *
 *-----------------------------------------------------------------*/

/*-------------------------------------------------------------------
 *
 *  Abstract:
 *
 *-----------------------------------------------------------------*/

#ifndef OPAL_ASYNC_MEM_H

#define OPAL_ASYNC_MEM_H

#if defined(__QNXNTO__)
	#include <semaphore.h>
	#include <inttypes.h>
#endif
#include <math.h>

//typedef unsigned char byte;
//typedef unsigned short word;

#if defined(__cplusplus)
extern "C"
{
#endif

// NOTE : Copié du fichier OpalRtMain.h
typedef enum
{
/*	0	*/	STATE_ERROR,
/*	1	*/	STATE_PAUSE,
/*	2	*/	STATE_LOAD,
/*	3	*/	STATE_RUN,
/*	4	*/	STATE_SINGLE_STEP,
/*	5	*/	STATE_RESET,
/*	6	*/	STATE_STOP,
/*	7	*/	STATE_WAIT_SC
} RUN_STATE;


typedef enum
{
	OP_ASYNC_WAIT_FOR_DATA,
	OP_ASYNC_PREPARE_DATA,
	OP_ASYNC_DATA_READY
} OpalAsyncStatusCode;


/* Send icon mode actually supported */
#define	NEED_REPLY_BEFORE_NEXT_SEND	0
#define	NEED_REPLY_NOW				1
#define	DONT_NEED_REPLY				2

/* Define to support multi-plateforme */
# ifndef UINT64_T
#  if defined(__i386__) && defined(__linux__)
#    define UINT64_T unsigned long long
#  endif
#  if defined(_MSC_VER)
#    define UINT64_T unsigned __int64
#  endif
#  if defined(__QNXNTO__)
#    define UINT64_T uint64_t
#  endif
# endif
# ifdef UINT64_T
#  if defined(__i386__) && defined(__linux__)
 __extension__
#  endif
# endif /* UINT64_T */


/****************************************************************************************
*
* Name :	OpalOpenAsyncMem
*
* Input Parameters :
*		sizeOfMemory :	the size in bytes of the shared memory.
*							
*		mem_name :		the name of shared memory.
*
* Output Parameters :	NIL
*		
* Return :	EOK		success
*			ENOMEM	Insufficient memory available.
*			EIO		Cannot open shared memory.
* 
* Description :	Open the shared memory for futur acces.
*
*****************************************************************************************/
int	OpalOpenAsyncMem(int	sizeOfMemory,const char *mem_name);


/****************************************************************************************
*
* Name :	OpalCloseAsyncMem
*
* Input Parameters :
*		sizeOfMemory :	Size of the shared memory.
*							
*		mem_name :		Name of shared memory.
*
* Output Parameters :	NIL
*		
* Return :	EOK		success
*			EIO		Cannot close shared memory.
* 
* Description :	Close the shared memory
*
*****************************************************************************************/
int	OpalCloseAsyncMem(int	sizeOfMemory,const char *mem_name);


/****************************************************************************************
*
* Name :	OpalGetNbAsyncSendIcon
*
* Input Parameters :	NIL
*
* Output Parameters :	
*		nbIcon :	pointer to a integer variable
*		
* Return :	EOK		success
* 
* Description :	Gives the number of send icon registered to the controller.
*
*****************************************************************************************/
int	OpalGetNbAsyncSendIcon(int	*nbIcon);


/****************************************************************************************
*
* Name :	OpalGetNbAsyncRecvIcon
*
* Input Parameters :	NIL
*
* Output Parameters :
*		nbIcon :	pointer to a integer variable
*
* Return :	EOK		success
* 
* Description :	Gives the number of receive icon registered to the controller.
*
*****************************************************************************************/
int	OpalGetNbAsyncRecvIcon(int	*nbIcon);


/****************************************************************************************
*
* Name :	OpalGetAsyncCtrlParameters
*
* Input Parameters :
*		size :		the size in bytes of the parameters structure.
*
* Output Parameters :	
*		ParamPtr :	pointer to a parameters structure.
*		
* Return :	EOK		success
*			E2BIG	size parameter are too big.
*			ENOEXEC pointer error, version error or shmem error
* 
* Description :	Give all the parameters of the controller icon.
*
*****************************************************************************************/
int	OpalGetAsyncCtrlParameters(void	*ParamPtr,int	size);


/****************************************************************************************
*
* Name :	OpalGetAsyncSendIconDataLength
*
* Input Parameters :	
*		SendID	:	ID of the icon
*
* Output Parameters :
*		length	:	pointer to a integer variable
*		
* Return :	EOK		success
*			EINVAL	invalid ID
*  
* Description :	Gives the data length of the send icon specified with SendID parameter.
*
*****************************************************************************************/
int	OpalGetAsyncSendIconDataLength(int	*length, unsigned int SendID);


/****************************************************************************************
*
* Name :	OpalGetAsyncSendIconMode
*
* Input Parameters :	
*		SendID	:	ID of the icon
*
* Output Parameters :
*		mode	:	pointer to a integer variable
*		
* Return :	EOK		success
*			EINVAL	invalid ID
*  
* Description :	Gives the mode of the send icon specified with SendID parameter.
*				(See at the top of this file for mode definition)
*
*****************************************************************************************/
int	OpalGetAsyncSendIconMode(int	*mode, unsigned int SendID);


/****************************************************************************************
*
* Name :	OpalGetAsyncRecvIconDataLength
*
* Input Parameters :	
*		RecvID	:	ID of the icon
*
* Output Parameters :
*		length	:	pointer to a integer variable
*		
* Return :	EOK		success
*			EINVAL	invalid ID
*  
* Description :	Gives the data length of the recv icon specified with RecvID parameter.
*
*****************************************************************************************/
int	OpalGetAsyncRecvIconDataLength(int	*length, unsigned int RecvID);


/****************************************************************************************
*
* Name :	OpalWaitForAsyncSendRequest
*
* Input Parameters :	NIL
*
* Output Parameters :
*		SendID	:	pointer to a integer variable
*		
* Return :	EOK		success
*			ENODEV	No send icon registered.
*  
* Description :	Gives the ID of the icon who call a send request.
*
*****************************************************************************************/
int	OpalWaitForAsyncSendRequest(unsigned int	*SendID);

/****************************************************************************************
*
* Name :	OpalAsyncSendRequestDone
*
* Input Parameters :	
*		SendID	:	ID of the icon
*
* Output Parameters :	nil
*		
* Return :	EOK		success
*			ENODEV	No send icon registered.
*			EINVAL	invalid ID
*  
* Description :	Reply to a specific send icon.
*
*****************************************************************************************/
int	OpalAsyncSendRequestDone(unsigned int	SendID);


/****************************************************************************************
*
* Name :	OpalGetAsyncSendIconData
*
* Input Parameters :	
*		size	:	size in bytes of the data buffer.
*		SendID	:	ID of the icon.
*
* Output Parameters :
*		dataPtr	:	pointer where the data are stored.
*		
* Return :	EOK		success
*			EINVAL	invalid ID
*			ENODEV	no send icon registered
*			E2BIG	size are too big.
*  
* Description :	Get the data for the specific send icon.
*
*****************************************************************************************/
int	OpalGetAsyncSendIconData(void	*dataPtr, int size, unsigned int SendID);

/****************************************************************************************
* Name :	OpalGetSubsetAsyncSendIconData
*
* Input Parameters :	
*		offset	:	offset in bytes of the data buffer to be get
*		size	:	size in bytes of the data buffer to get.
*		SendID	:	ID of the icon.
*
* Output Parameters :
*		dataPtr	:	pointer where the data are stored.
*		
* Return :	EOK		succes
*			EINVAL	invalid ID
*			ENODEV	no send icon registered
*			E2BIG	size are too big.
*  
* Description :	Get one subset of the data for the specific send icon.
*               OpalGetSubsetAsyncSendIconData(buf,0,size,sendId) 
*				is the same as OpalGetAsyncSendIconData(buf,size,sendId) 
*
*****************************************************************************************/

int OpalGetSubsetAsyncSendIconData(double *dataPtr, int offset, int size, unsigned int SendID);




/****************************************************************************************
*
* Name :	OpalSetAsyncRecvIconData
*
* Input Parameters :	
*		size	:	size in bytes of the data buffer.
*		RecvID	:	ID of the icon.
*
* Output Parameters :
*		dataPtr	:	pointer where the data are stored.
*		
* Return :	EOK		success
*			EINVAL	invalid ID
*			ENODEV	no send icon registered
*			E2BIG	size are too big.
*  
* Description :	Set the data for the specific recv icon.
*
*****************************************************************************************/
int	OpalSetAsyncRecvIconData(void	*dataPtr, int size, unsigned int RecvID);

/****************************************************************************************
*
* Name :	OpalSetSubsetAsyncRecvIconData
*
* Input Parameters :	
*		offset	:	offset in bytes of the data buffer to be set
*		size	:	size in bytes of the data buffer.to be set
*		RecvID	:	ID of the icon.
*
* Output Parameters :
*		dataPtr	:	pointer where the data are stored.
*		
* Return :	EOK		succes
*			EINVAL	invalid ID
*			ENODEV	no send icon registered
*			E2BIG	size are too big.
*  
* Description :	Set one subset of the data for the specific recv icon.
*               OpalSetSubsetAsyncRecvIconData(buf,0,size,recvId) 
*				is the same as OpalSetAsyncRecvIconData(buf,size,recvId) 
*
*****************************************************************************************/
int	OpalSetSubsetAsyncRecvIconData(double *dataPtr,int offset,int size, unsigned int RecvID);


/****************************************************************************************
*
* Name :	OpalSetAsyncSendIconError
*
* Input Parameters :	
*		Error	:	error code returned to the send icon.
*		SendID	:	ID of the icon.
*
* Output Parameters :	NIL
*		
* Return :	EOK		success
*			EINVAL	invalid ID
*			ENODEV	no send icon registered
*  
* Description :	Set the error code for the specific send icon.
*
*****************************************************************************************/
int	OpalSetAsyncSendIconError(double Error, unsigned int SendID);


/****************************************************************************************
*
* Name :	OpalSetAsyncRecvIconError
*
* Input Parameters :	
*		Error	:	error code returned to the receive icon.
*		RecvID	:	ID of the icon.
*
* Output Parameters :	NIL
*		
* Return :	EOK		success
*			EINVAL	invalid ID
*			ENODEV	no receive icon registered
*  
* Description :	Set the error code for the specific receive icon.
*
*****************************************************************************************/
int	OpalSetAsyncRecvIconError(double Error, unsigned int RecvID);


/****************************************************************************************
*
* Name :	OpalSetAsyncRecvIconStatus
*
* Input Parameters :	
*		Status	:	status code returned to the recveive icon.
*		RecvID	:	ID of the icon.
*
* Output Parameters :	NIL
*		
* Return :	EOK		success
*			EINVAL	invalid ID
*			ENODEV	no receive icon registered
*  
* Description :	Set the status code for the specific receive icon.
*
*****************************************************************************************/
int	OpalSetAsyncRecvIconStatus(double Status, unsigned int RecvID);

/****************************************************************************************
*
* Name :	OpalSetAsyncRecvIconStatus
*
* Input Parameters :	
*		Status	:	status code returned to the recveive icon.
*		RecvID	:	ID of the icon.
*
* Output Parameters :	NIL
*		
* Return :	EOK		success
*			EINVAL	invalid ID
*			ENODEV	no receive icon registered
*  
* Description :	Set the status code for the specific receive icon.
*
*****************************************************************************************/
int	OpalSetAsyncRecvIconStatusMult(double Status, unsigned int RecvID);

/****************************************************************************************
*
* Name :	OpalGetAsyncRecvIconTimeout
*
* Input Parameters :	
*		RecvID	:	ID of the icon.
*
* Output Parameters :
*		timeout	:	pointer to a double.
*		
* Return :	EOK		success
*			EINVAL	invalid ID
*			ENODEV	no recv icon registered
*  
* Description :	Get the timout for the specific receive icon.
*
*****************************************************************************************/
int	OpalGetAsyncRecvIconTimeout(double	*timeout, unsigned int RecvID);

/****************************************************************************************
*
* Name :	OpalGetAsyncSendIDList
*
* Input Parameters :	
*		size	:	size in bytes of the data buffer.
*
* Output Parameters :
*		dataPtr	:	pointer where the data are stored.
*		
* Return :	EOK		success
*			ENODEV	no send icon registered
*			E2BIG	invalid size.
*  
* Description :	Get the ID list of all Send icon.
*
*****************************************************************************************/
int	OpalGetAsyncSendIDList(void	*dataPtr, int size);


/****************************************************************************************
*
* Name :	OpalGetAsyncRecvIDList
*
* Input Parameters :	
*		size	:	size in bytes of the data buffer.
*
* Output Parameters :
*		dataPtr	:	pointer where the data are stored.
*		
* Return :	EOK		success
*			ENODEV	no Recv icon registered
*			E2BIG	invalid size.
*  
* Description :	Get the ID list of all Recv icon.
*
*****************************************************************************************/
int	OpalGetAsyncRecvIDList(void	*dataPtr, int size);

/****************************************************************************************
*
* Name :	OpalGetAsyncSendParameters
*
* Input Parameters :	
*		size	:	size in bytes of the data buffer.
*		SendID	:	ID of the icon.
*
* Output Parameters :
*		dataPtr	:	pointer where the data are stored.
*		
* Return :	EOK		success
*			EINVAL	invalid ID
*			ENODEV	no send icon registered
*  
* Description :	Get all parameters of the specific Send icon.
*
*****************************************************************************************/
int	OpalGetAsyncSendParameters(void	*dataPtr, int size, unsigned int SendID);
/****************************************************************************************
*
* Name :	OpalGetAsyncSendExtFloatParameters
*
* Input Parameters :	
*		size	:	size in bytes of the data buffer.
*		SendID	:	ID of the icon.
*
* Output Parameters :
*		dataPtr	:	pointer where the exterended is to be stored.
*		
* Return :	number of parameters
*  
* Description :	Get all extended parameters of the specific Send icon.
*
*****************************************************************************************/
int	OpalGetAsyncSendExtFloatParameters(void	*dataPtr, int size, unsigned int SendID);

/****************************************************************************************
*
* Name :	OpalGetAsyncRecvParameters
*
* Input Parameters :	
*		size	:	size in bytes of the data buffer.
*		RecvID	:	ID of the icon.
*
* Output Parameters :
*		dataPtr	:	pointer where the data are stored.
*		
* Return :	EOK		success
*			EINVAL	invalid ID
*			ENODEV	no send icon registered
*  
* Description :	Get all parameters of the specific Recv icon.
*
*****************************************************************************************/
int	OpalGetAsyncRecvParameters(void	*dataPtr, int size, unsigned int RecvID);
/****************************************************************************************
*
* Name :	OpalGetAsyncRecvExtFloatParameters
*
* Input Parameters :	
*		size	:	size in bytes of the data buffer.
*		SendID	:	ID of the icon.
*
* Output Parameters :
*		dataPtr	:	pointer where the exterended is to be stored.
*		
* Return :	number of parameters
*  
* Description :	Get all extended parameters of the specific recv icon.
*
*****************************************************************************************/
int	OpalGetAsyncRecvExtFloatParameters(void	*dataPtr, int size, unsigned int RecvID);
/****************************************************************************************
*
* Name :	OpalGetAsyncCtrlExtFloatParameters
*
* Input Parameters :	
*		size	:	size in bytes of the data buffer.
*		SendID	:	ID of the icon.
*
* Output Parameters :
*		dataPtr	:	pointer where the exterended is to be stored.
*		
* Return :	number of parameters
*  
* Description :	Get all extended parameters of the specific ctrl icon.
*
*****************************************************************************************/
int	OpalGetAsyncCtrlExtFloatParameters(void	*dataPtr, int size);

/****************************************************************************************
*
* Name :	OpalGetAsyncModelState 
*
* Input Parameters :	NIL
*
* Output Parameters :	NIL
*		
* Return :		STATE_ERROR			
*				STATE_PAUSE			
*				STATE_LOAD			
*				STATE_RUN			
*				STATE_SINGLE_STEP	
*				STATE_RESET			
*				STATE_STOP			
*				STATE_WAIT_SC		
*  
* Description :	Returns the state of the model
*
*****************************************************************************************/
int OpalGetAsyncModelState (void);

// RT is defined coz these prototypes are reuired only on target site
// and long long are not supported on Windows
#if defined(RT)
/****************************************************************************************
*
* Name :	OpalGetAsyncStartExecCpuTime 
*
* Input Parameters :	NIL
*
* Output Parameters :	NIL
*		
* Return :		
* Description :	This function returns the cuurent CPU time counter read at start of execution
*
*****************************************************************************************/
int OpalGetAsyncStartExecCpuTime( void * ParamPtr, UINT64_T * time );

/****************************************************************************************
*
* Name :	OpalGetAsyncStartPauseCpuTime 
*
* Input Parameters :	NIL
*
* Output Parameters :	NIL
*		
* Return :		
* Description :	This function returns the cuurent CPU time counter read at start of execution
*
*****************************************************************************************/
int OpalGetAsyncStartPauseCpuTime( void * ParamPtr, UINT64_T * time );
/****************************************************************************************
*
* Name :	OpalGetAsyncModelTime
*
* Input Parameters :	NIL
*
* Output Parameters :	NIL
*		
* Return :		
* Description :	This function get a CPU time close to model sync, and the current modelTime
*
*****************************************************************************************/
int OpalGetAsyncModelTime( void * ParamPtr, unsigned long long * CPUtime, double * modelTime);
#endif

unsigned atoh (const char * ptr);
unsigned ascii_to_hexa (char * str);

#if defined(__cplusplus)
}
#endif

#endif  // #ifndef OPAL_ASYNC_MEM_H

