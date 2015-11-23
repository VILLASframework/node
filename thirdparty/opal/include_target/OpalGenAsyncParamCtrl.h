/*-------------------------------------------------------------------
 *	OPAL-RT Technologies inc
 *
 *	Copyright (C) 1999. All rights reserved.
 *
 *	File name =			$Workfile: OpalGenAsyncParamCtrl.h $
 *	SourceSafe path =	$Logfile: /SIMUPAR/soft/common/include_target/OpalGenAsyncParamCtrl.h $
 *	SourceSafe rev. =	$Revision: 2.8 $
 *	Last checked in =	$Date: 2009/12/15 14:42:27 $
 *	Last updated =		$Modtime: 7/31/02 10:25a $
 *	Last modified by =	$Author: AntoineKeirsbulck $
 *
 *-----------------------------------------------------------------*/

/*-------------------------------------------------------------------
 *
 *  Abstract:
 *
 *-----------------------------------------------------------------*/

#include "OpalTypes.h"

#ifndef OPAL_GENASYCPARAM_CTRL_H
#define OPAL_GENASYCPARAM_CTRL_H

#define	SENDASYNC_NB_FLOAT_PARAM		5
#define	SENDASYNC_NB_STRING_PARAM		5
#define	SENDASYNC_MAX_STRING_LENGTH		1000

#define	RECVASYNC_NB_FLOAT_PARAM		5
#define	RECVASYNC_NB_STRING_PARAM		5
#define	RECVASYNC_MAX_STRING_LENGTH		1000

#define	GENASYNC_NB_FLOAT_PARAM			12
#define	GENASYNC_NB_STRING_PARAM		12
#define	GENASYNC_MAX_STRING_LENGTH		1000


// Align bytes
#if defined(__GNUC__)
#	undef	GNUPACK
#	define	GNUPACK(x)		__attribute__ ((aligned(x),packed))
#else
#	undef	GNUPACK
#	define	GNUPACK(x)
#	if defined(__sgi)
#		pragma pack(1)
#	else
#		pragma pack (push, 1)
#	endif
#endif


//------------ LocalData ---------------
typedef struct
{
	unsigned char	controllerID														;// ignored GNUPACK(1);
	double			FloatParam[GENASYNC_NB_FLOAT_PARAM]									GNUPACK(1);
	char			StringParam[GENASYNC_NB_STRING_PARAM][GENASYNC_MAX_STRING_LENGTH]	;// ignored GNUPACK(1);
	UINT64_T		execStartCpuTime													GNUPACK(1);
	UINT64_T		pauseStartCpuTime													GNUPACK(1);
#ifdef RT
	unsigned long long	modelSyncCpuTime												GNUPACK(1);
#else
	UINT64_T		modelSyncCpuTime													GNUPACK(1);
#endif
	double			modelTime															GNUPACK(1);

} Opal_GenAsyncParam_Ctrl;


typedef struct
{
	double			FloatParam[SENDASYNC_NB_FLOAT_PARAM]								GNUPACK(1);
	char			StringParam[SENDASYNC_NB_STRING_PARAM][SENDASYNC_MAX_STRING_LENGTH]	;// ignored GNUPACK(1);

} Opal_SendAsyncParam;

typedef struct
{
	double			FloatParam[RECVASYNC_NB_FLOAT_PARAM]								GNUPACK(1);
	char			StringParam[RECVASYNC_NB_STRING_PARAM][RECVASYNC_MAX_STRING_LENGTH]	;// ignored GNUPACK(1);

} Opal_RecvAsyncParam;

#if defined(__GNUC__)
#	undef	GNUPACK
#else
#	if defined(__sgi)
#		pragma pack(0)
#	else
#		pragma pack (pop)
#	endif
#endif

#endif  // #ifndef OPAL_GENASYCPARAM_CTRL_H
