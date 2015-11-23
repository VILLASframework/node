/*-------------------------------------------------------------------
 *	OPAL-RT Technologies inc
 *
 *	Copyright (C) 1999. All rights reserved.
 *
 *	File name =			$Workfile: OpalPrint.h $
 *	SourceSafe path =	$Logfile: /SIMUPAR/soft/common/include_target/OpalPrint.h $
 *	SourceSafe rev. =	$Revision: 2.3 $
 *	Last checked in =	$Date: 2004/08/13 19:35:48 $
 *	Last updated =		$Modtime: 03-10-14 16:58 $Author: djibriln $
 *
 *-----------------------------------------------------------------*/

#ifndef OPALPRINT_H
#define OPALPRINT_H

#if defined(__cplusplus)
extern "C"
{
#endif

#if defined (RTLAB) && !defined(IS_COMPILED_IN_CONTROLLER)  && !defined(IS_COMPILED_IN_METACONTROLLER)
	int	OpalPrint(const char *format, ... );
#else
	#define OpalPrint printf
#endif


	/****************************************************************************************
	*
	* Name :	OpalSystemCtrl_Register
	*
	* Input Parameters :
	*		sysShMemName :	The name of shared memory.
	*							
	* Output Parameters :	NIL
	*		
	* Return :	EOK		Succes
	*			-1		Cannot open shared memory.
	* 
	* Description :	Enable the OpalPrint function to send message via the OpalDisplay by opening a shared memory.
	*
	*		NOTE:	RTLAB must be defined before including this file (OpalPrint.h)
	*
	*****************************************************************************************/	
	int	OpalSystemCtrl_Register(char *sysShMemName); 

	/****************************************************************************************
	*
	* Name :	OpalSystemCtrl_UnRegister
	*
	* Input Parameters :
	*		sysShMemName :	The name of shared memory.
	*							
	* Output Parameters :	NIL
	*		
	* Return :	EOK		Succes
	* 
	* Description :	Close the OpalPrint shared memory.
	*
	*		NOTE:	RTLAB must be defined before including this file (OpalPrint.h)
	*
	*****************************************************************************************/	
	int OpalSystemCtrl_UnRegister(char *sysMemName);


	/****************************************************************************************
	*
	* Name :	OpalSendFileName
	*
	* Input Parameters :
	*		fileName :	Name of the file to be retrieved on reset
	*		fileMode :  Specify 'a' for "Ascii", 'b' for "Binary"
	*		fileType :  Set to 0. For internal use only.
	*							
	* Output Parameters :	NIL
	*		
	* Return :	EOK		Succes
	* 
	* Description :	Sends the name and type of a file so that RT-LAB retrieves it on reset.
	*
	*		NOTE:	RTLAB must be defined before including this file (OpalPrint.h)
	*
	*****************************************************************************************/	
	int	OpalSendFileName(char *fileName, char fileMode, int fileType);

	/****************************************************************************************
	*
	* Name :	OpalSendClosedFileName
	*
	* Input Parameters :
	*		fileName :	Name of the file to be retrieved
	*
	* Output Parameters :	NIL
	*
	* Return :	EOK		Succes
	*
	* Description :	Sends the name of a file ready to be retrieved.
	*
	*		NOTE:	RTLAB must be defined before including this file (OpalPrint.h)
	*
	*****************************************************************************************/
	int	OpalSendClosedFileName(char *fileName);


#if defined(__cplusplus)
}
#endif

#endif
