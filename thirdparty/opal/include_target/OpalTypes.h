/*-------------------------------------------------------------------
 *	OPAL-RT Technologies inc
 *
 *	Copyright (C) 1999. All rights reserved.

 *	File name =			$RCSfile: OpalTypes.h,v $
 *	CVS  path =			$Source: /git/RT-LAB/RT-LAB/soft/common/include_target/OpalTypes.h,v $
 *	CVS  rev. =			$Revision: 1.8 $
 *	Last checked in =	$Date: 2010/07/07 15:53:08 $
 *	Last modified by =	$Author: AntoineKeirsbulck $
 *
 *-----------------------------------------------------------------*/

/*-------------------------------------------------------------------
 *
 *  Abstract:
 *      Data types for use with MATLAB/SIMULINK and the Real-Time Workshop.
 *      Data types for use with XMATH/SYSTEMBUILD and AutoCode.
 *
 *      When compiling stand-alone model code, data types can be overridden
 *      via compiler switches.
 *
 *-----------------------------------------------------------------*/

#ifndef __OPALTYPES__
#define __OPALTYPES__

#ifdef _WIN32
	#ifndef WIN32
	#define WIN32
	#endif
#endif

#if defined(__QNXNTO__)
#	include <inttypes.h>
#endif

/* =========================================================================
	MATLAB
	Inclure les types de tmwtypes.h (de Matlab) seulement si ce dernier
	n'a pas déjà été inclus, sinon on a des erreurs de redefinition
	avec gcc. Tous ce qui est inclus entre le #if qui suit et le #endif
	correspondant est une copie du contenu de tmwtypes.h. Pour inclure
	d'autre types, s'assurer de les mettre en dehors de la région délimitée
	par ces #if __TMWTYPES__ / #endif
*/
#if ! defined(__TMWTYPES__)
#include <limits.h>

#ifndef __MWERKS__
# ifdef __STDC__
#  include <float.h>
# else
#  define FLT_MANT_DIG 24
#  define DBL_MANT_DIG 53
# endif
#endif

/*
 *      The following data types cannot be overridden when building MEX files.
 */
#ifdef MATLAB_MEX_FILE
# undef CHARACTER_T
# undef INTEGER_T
# undef BOOLEAN_T
# undef REAL_T
# undef TIME_T
#endif

/*
 *      The following define is used to emulate when all integer types are
 *      32-bits.  This is the case for TI C30/C40 DSPs which are RTW targets.
 */
#ifdef DSP32
# define INT8_T    int
# define UINT8_T   unsigned int
# define INT16_T   int
# define UINT16_T  unsigned int
#endif

/*
 * The uchar_T, ushort_T and ulong_T types are needed for compilers which do
 * not allow defines to be specified, at the command line, with spaces in them.
 */

typedef unsigned char  uchar_T;
typedef unsigned short ushort_T;
typedef unsigned long  ulong_T;



/*=======================================================================*
 * Fixed width word size data types:                                     *
 *   int8_T, int16_T, int32_T     - signed 8, 16, or 32 bit integers     *
 *   uint8_T, uint16_T, uint32_T  - unsigned 8, 16, or 32 bit integers   *
 *   real32_T, real64_T           - 32 and 64 bit floating point numbers *
 *=======================================================================*/

#ifndef INT8_T
# if CHAR_MIN == -128
#   define INT8_T char
# elif SCHAR_MIN == -128
#   define INT8_T signed char
# endif
#endif
#ifdef INT8_T
  typedef INT8_T int8_T;
# ifndef UINT8_T
#   define UINT8_T unsigned char
# endif
  typedef UINT8_T uint8_T;
#endif


#ifndef INT16_T
# if SHRT_MAX == 0x7FFF
#  define INT16_T short
# elif INT_MAX == 0x7FFF
#  define INT16_T int
# endif
#endif
#ifdef INT16_T
 typedef INT16_T int16_T;
#endif


#ifndef UINT16_T
# if SHRT_MAX == 0x7FFF    /* can't compare with USHRT_MAX on some platforms */
#  define UINT16_T unsigned short
# elif INT_MAX == 0x7FFF
#  define UINT16_T unsigned int
# endif
#endif
#ifdef UINT16_T
  typedef UINT16_T uint16_T;
#endif


#ifndef INT32_T
# if INT_MAX == 0x7FFFFFFF
#  define INT32_T int
# elif LONG_MAX == 0x7FFFFFFF
#  define INT32_T long
# endif
#endif
#ifdef INT32_T
 typedef INT32_T int32_T;
#endif


#ifndef UINT32_T
# if INT_MAX == 0x7FFFFFFF
#  define UINT32_T unsigned int
# elif LONG_MAX == 0x7FFFFFFF
#  define UINT32_T unsigned long
# endif
#endif
#ifdef UINT32_T
 typedef UINT32_T uint32_T;
#endif


#ifndef REAL32_T
# ifndef __MWERKS__
#  if FLT_MANT_DIG >= 23
#    define REAL32_T float
#  endif
# else
#    define REAL32_T float
# endif
#endif
#ifdef REAL32_T
typedef REAL32_T real32_T;
#endif

#ifndef REAL64_T
# ifndef __MWERKS__
#  if DBL_MANT_DIG >= 52
#    define REAL64_T double
#  endif
# else
#    define REAL64_T double
# endif
#endif
#ifdef REAL64_T
  typedef REAL64_T real64_T;
#endif

/*=======================================================================*
 * Fixed width word size data types:                                     *
 *   int64_T                      - signed 64 bit integers               *
 *   uint64_T                     - unsigned 64 bit integers             *
 *=======================================================================*/

#ifndef INT64_T
# if defined(__alpha) || (defined(_MIPS_SZLONG) && (_MIPS_SZLONG == 64))
#   define INT64_T long
#   define FMT64 ""
# endif
# if defined(__LP64__)
#   define INT64_T long
#   define FMT64 "L"
# endif
# if defined(__i386__) && defined(__linux__)
#   define INT64_T long long
#   define FMT64 "L"
# endif
# if defined(_MSC_VER)
#   define INT64_T __int64
#   define FMT64 "I64"
# endif
# if defined(__QNXNTO__)
#   define INT64_T int64_t
#  endif
#endif
#ifdef INT64_T
# if defined(__i386__) && defined(__linux__)
 __extension__
# endif
 typedef INT64_T int64_T;
#endif


#ifndef UINT64_T
# if defined(__alpha) || (defined(_MIPS_SZLONG) && (_MIPS_SZLONG == 64))
#   define UINT64_T unsigned long
# endif
# if defined(__LP64__)
#   define UINT64_T unsigned long
# endif
# if defined(__i386__) && defined(__linux__)
#   define UINT64_T unsigned long long
# endif
# if defined(_MSC_VER)
#   define UINT64_T unsigned __int64
# endif
# if defined(__QNXNTO__)
#   define UINT64_T uint64_t
#endif
#endif
#ifdef UINT64_T
# if defined(__i386__) && defined(__linux__)
 __extension__
# endif
 typedef UINT64_T uint64_T;
#endif


/*================================================================*
 *  Fixed-point data types:                                       *
 *     fixpoint_T     - 16 or 32-bit unsigned integers            *
 *     sgn_fixpoint_T - 16 or 32-bit signed integers              *
 *  Note, when building fixed-point applications, real_T is equal *
 *  to fixpoint_T and time_T is a 32-bit unsigned integer.        *
 *================================================================*/

#ifndef FIXPTWS
# define FIXPTWS 32
#endif
#if FIXPTWS != 16 && FIXPTWS != 32
  "--> fixed-point word size (FIXPTWS) must be 16 or 32 bits"
#endif

#if FIXPTWS == 16
  typedef uint16_T fixpoint_T;
  typedef int16_T  sgn_fixpoint_T;
#else
  typedef uint32_T fixpoint_T;
  typedef int32_T  sgn_fixpoint_T;
#endif

#ifdef FIXPT
# define REAL_T fixpoint_T
# define TIME_T uint32_T
#endif



/*===========================================================================*
 * General or logical data types where the word size is not guaranteed.      *
 *  real_T     - possible settings include real32_T, real64_T, or fixpoint_T *
 *  time_T     - possible settings include real64_T or uint32_T              *
 *  boolean_T                                                                *
 *  char_T                                                                   *
 *  int_T                                                                    *
 *  uint_T                                                                   *
 *  byte_T                                                                   *
 *===========================================================================*/

#ifndef REAL_T
# ifdef REAL64_T
#   define REAL_T real64_T
# else
#   ifdef REAL32_T
#     define REAL_T real32_T
#   endif
# endif
#endif
#ifdef REAL_T
  typedef REAL_T real_T;
#endif

#ifndef TIME_T
#  ifdef REAL_T
#    define TIME_T real_T
#  endif
#endif
#ifdef TIME_T
  typedef TIME_T time_T;
#endif

#ifndef BOOLEAN_T
# if defined(UINT8_T)
#   define BOOLEAN_T UINT8_T
# else
#   define BOOLEAN_T unsigned int
# endif
#endif
typedef BOOLEAN_T boolean_T;


#ifndef CHARACTER_T
#define CHARACTER_T char
#endif
typedef CHARACTER_T char_T;


#ifndef INTEGER_T
#define INTEGER_T int
#endif
typedef INTEGER_T int_T;


#ifndef UINTEGER_T
#define UINTEGER_T unsigned
#endif
typedef UINTEGER_T uint_T;


#ifndef BYTE_T
#define BYTE_T unsigned char
#endif
typedef BYTE_T byte_T;


/*===========================================================================*
 * Define Complex Structures                                                 *
 *===========================================================================*/

#ifndef CREAL32_T
#  ifdef REAL32_T
    typedef struct {
      real32_T re, im;
    } creal32_T;
#    define CREAL32_T creal32_T
#  endif
#endif

#ifndef CREAL64_T
#  ifdef REAL64_T
    typedef struct {
      real64_T re, im;
    } creal64_T;
#    define CREAL64_T creal64_T
#  endif
#endif

#ifndef CREAL_T
#  ifdef REAL_T
    typedef struct {
      real_T re, im;
    } creal_T;
#    define CREAL_T creal_T
#  endif
#endif

#ifndef CINT8_T
#  ifdef INT8_T
    typedef struct {
      int8_T re, im;
    } cint8_T;
#    define CINT8_T cint8_T
#  endif
#endif

#ifndef CUINT8_T
#  ifdef UINT8_T
    typedef struct {
      uint8_T re, im;
    } cuint8_T;
#    define CUINT8_T cuint8_T
#  endif
#endif

#ifndef CINT16_T
#  ifdef INT16_T
    typedef struct {
      int16_T re, im;
    } cint16_T;
#    define CINT16_T cint16_T
#  endif
#endif

#ifndef CUINT16_T
#  ifdef UINT16_T
    typedef struct {
      uint16_T re, im;
    } cuint16_T;
#    define CUINT16_T cuint16_T
#  endif
#endif

#ifndef CINT32_T
#  ifdef INT32_T
    typedef struct {
      int32_T re, im;
    } cint32_T;
#    define CINT32_T cint32_T
#  endif
#endif

#ifndef CUINT32_T
#  ifdef UINT32_T
    typedef struct {
      uint32_T re, im;
    } cuint32_T;
#    define CUINT32_T cuint32_T
#  endif
#endif

#else /* defined(__TMWTYPES__) */

#ifdef OP_MATLABR12

#ifndef INT64_T
# if defined(__alpha) || (defined(_MIPS_SZLONG) && (_MIPS_SZLONG == 64))
#   define INT64_T long
#   define FMT64 ""
# endif
# if defined(__LP64__)
#   define INT64_T long
#   define FMT64 "L"
# endif
# if defined(__i386__) && defined(__linux__)
#   define INT64_T long long
#   define FMT64 "L"
# endif
# if defined(_MSC_VER)
#   define INT64_T __int64
#   define FMT64 "I64"
# endif
# if defined(__QNXNTO__)
#   define INT64_T int64_t
#  endif
#endif
#ifdef INT64_T
# if defined(__i386__) && defined(__linux__)
 __extension__
# endif
 typedef INT64_T int64_T;
#endif

# ifndef UINT64_T
#  if defined(__alpha) || (defined(_MIPS_SZLONG) && (_MIPS_SZLONG == 64))
#    define UINT64_T unsigned long
#  endif
# if defined(__LP64__)
#   define UINT64_T unsigned long
# endif
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
typedef UINT64_T uint64_T;
# endif /* UINT64_T */
#endif /* OP_MATLABR12 */

#endif /* ! defined(_	__) */
/*
  MATLAB
  ===========================================================================
*/

/*=======================================================================*
 * Min and Max:                                                          *
 *   int8_T, int16_T, int32_T     - signed 8, 16, or 32 bit integers     *
 *   uint8_T, uint16_T, uint32_T  - unsigned 8, 16, or 32 bit integers   *
 *=======================================================================*/

#define  OP_MAX_int8_T      ((int8_T)(127))            /* 127  */
#define  OP_MIN_int8_T      ((int8_T)(-128))           /* -128 */
#define  OP_MAX_uint8_T     ((uint8_T)(255))           /* 255  */
#define  OP_MIN_uint8_T     ((uint8_T)(0))

#define  OP_MAX_int16_T     ((int16_T)(32767))         /* 32767 */
#define  OP_MIN_int16_T     ((int16_T)(-32768))        /* -32768 */
#define  OP_MAX_uint16_T    ((uint16_T)(65535))        /* 65535 */
#define  OP_MIN_uint16_T    ((uint16_T)(0))

#define  OP_MAX_int32_T     ((int32_T)(2147483647))    /* 2147483647  */
#define  OP_MIN_int32_T     ((int32_T)(-2147483647-1)) /* -2147483648 */
#define  OP_MAX_uint32_T    ((uint32_T)(0xFFFFFFFFU))  /* 4294967295  */
#define  OP_MIN_uint32_T    ((uint32_T)(0))

#ifdef INT64_T
#define  OP_MAX_int64_T     ((int64_T)(9223372036854775807))
#define  OP_MIN_int64_T     ((int64_T)(-9223372036854775807-1))
#endif
#ifdef UINT64_T
#define  OP_MAX_uint64_T    ((uint64_T)(0xFFFFFFFFFFFFFFFFULL))
#define  OP_MIN_uint64_T    ((uint64_T)(0))
#endif

/*
*
*	Equivalent type between SIMULINK and SYSTEMBUILD
*
*/
#ifndef _SA_TYPES
#define _SA_TYPES
	typedef	real_T		RT_FLOAT;
	typedef int_T		RT_INTEGER;
	typedef boolean_T	RT_BOOLEAN;
	typedef time_T		RT_DURATION;
#endif	/* _SA_TYPES */


#ifndef FALSE
	#define FALSE       0
#endif
#ifndef TRUE
	#define TRUE        1
#endif


#ifdef INT64_T
#	if ! defined(LONGLONG_MAX)
#		define LONGLONG_MAX		OP_MAX_int64_T
#	endif
#	if ! defined(LONGLONG_MIN)
#		define LONGLONG_MIN		OP_MIN_int64_T
#	endif
#endif
#ifdef UINT64_T
#	if ! defined(ULONGLONG_MAX)
#		define ULONGLONG_MAX	OP_MAX_uint64_T
#	endif
#endif


// Some define for partability
#if defined(WIN32)	/* =============================================================== */

	typedef unsigned int		pid_t;
	typedef int					pthread_t;
#	ifndef PATH_MAX
#		define PATH_MAX (MAX_PATH)
#	endif
#	define OP_SLEEP( msec )		Sleep( msec )
#	define pthread_self			GetCurrentThreadId
#	define vsnprintf			_vsnprintf
#	define snprintf				_snprintf
#	define FILE_ALL_RW_PERMS	(_S_IWRITE | _S_IREAD)

#elif defined(__QNX__)	/* =========================================================== */

#	if ! defined(_IEEE1394_H_)
	typedef int				HANDLE;
#	endif /*  ! _IEEE1394_H_ */
	typedef int				HMODULE;

#	if defined(__QNXNTO__)

#		define _MAX_PATH	(PATH_MAX)
//#ifndef _strdup
//#		define _strdup(s)	strdup(s)
//#endif
#		define _strlwr(s)	strlwr(s)

#	else /*  ! __QNXNTO  */

		typedef int				pthread_t;
#		define pthread_self		getpid

#	endif /* __QNXNTO__  */

#	define MAX_PATH (PATH_MAX)
#	define OP_SLEEP( msec )		delay( msec )
//#	define _getcwd(b,l)			getcwd(b,l)

#	define FILE_ALL_RW_PERMS	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)


#elif defined(__linux__)	/* ======================================================= */

	typedef int				HANDLE;
	typedef int				HMODULE;
#	define MAX_PATH (PATH_MAX)
#	define _MAX_PATH (PATH_MAX)

#	include <sys/time.h>
#	define OP_SLEEP( msec )						\
	{	struct timeval tv;						\
		tv.tv_sec = (msec) / 1000;				\
		tv.tv_usec = ((msec) % 1000) * 1000;	\
		select(0, NULL, NULL, NULL, &tv);		\
	}

//#ifndef _strdup
//#	define _strdup(s)			strdup(s)
//#endif
//#	define _getcwd(b,l)			getcwd(b,l)
//#	define _access(s1,s2)		access(s1,s2)
//#	define _alloca(s)			alloca(s)
//#	define _mkdir(s)			mkdir(s)
#	define stricmp(s1,s2)		strcasecmp(s1,s2)
#	define strnicmp(s1,s2,n)	strncasecmp(s1,s2,n)
#	define FILE_ALL_RW_PERMS	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

#elif defined(__sun__) || defined(__sgi)
	typedef int HANDLE;
	typedef int	HMODULE;
#	define MAX_PATH (PATH_MAX)
#	define _MAX_PATH (PATH_MAX)

#	define	stricmp(s1, s2)			strcasecmp(s1, s2)
#	define	strnicmp(s1, s2, len)	strncasecmp(s1, s2, len)
//#	define	_strdup(s)				strdup(s)
//#	define _getcwd(b,l)				getcwd(b,l)
//#	define _alloca(s)				alloca(s)

#endif /*  WIN32  /  __QNX__  /  __linux__  //  __sun__  // __sgi  //======================================= */

//#ifndef WIN32
//#include <string.h>
//#include <strings.h>
//#include <sys/stat.h>
//#include <unistd.h>

//#ifdef _strdup
//#undef _strdup
//#warning _strdup should not be #defined!
//#endif
//
//# ifndef __QNX__
//	static inline int stricmp(char const * s1, char const *s2) {
//		return strcasecmp(s1,s2);
//	}
//	static inline int strnicmp(char const * s1, char const *s2, size_t l) {
//		return strncasecmp(s1,s2,l);
//	}
//// QDT: Not possible due o libOpalCore and libOpalOhci depending on each other
////	static inline int _stricmp(char const * s1, char const *s2) {
////		return strcasecmp(s1,s2);
////	}
//#  ifndef _stricmp
//#	define _stricmp stricmp
//#  endif
//	static inline int _strnicmp(char const * s1, char const *s2, size_t l) {
//		return strncasecmp(s1,s2,l);
//	}
//	static inline char * _strdup(const char * s) {
//		return strdup(s);
//	}
////	static inline void * _alloca(size_t size) {
////		return alloca(size);
////	}
//# endif
//	static inline pid_t _getpid() {
//		return getpid();
//	}
//	static inline char * _getcwd(char * buffer, size_t size) {
//		return getcwd(buffer, size);
//	}
//	static inline int _mkdir(const char *path, mode_t mode) {
//		return mkdir(path, mode);
//	}
//	static inline int _access(const char *pathname, int mode) {
//		return access(pathname, mode);
//	}
//	static inline int _close(int fildes) {
//		return close(fildes);
//	}
//#endif

typedef struct
{
	union
	{
		struct { unsigned char s_b1,s_b2,s_b3,s_b4; } S_un_b;
		uint32_T	S_addr;
	} S_un;
} OP_IN_ADDR;


typedef enum
{
	TR_PRIORITY_NORMAL = 0,
	TR_PRIORITY_HIGH,
	TR_MAX_PRIORITY

}	TR_PRIORITY;

#define ROUND_ST(x)	(int)((x)+0.5)
#define FLOAT_EQU(x,y) (((((x)-(y)) >= 0) && (((x)-(y)) < 0.0000001)) || ((((y)-(x)) >= 0) && (((y)-(x)) < 0.0000001))  )

#if ! defined(OP_API_H)
typedef	unsigned short	OP_API_INSTANCE_ID;
typedef unsigned long OP_LOAD_ID;
#endif

#if ! defined(EOK)
#	define	EOK	0
#else
#	if EOK != 0
	Generated error: EOK should be 0
#	endif
#endif

#define NUM_MODEL_ARGS	8

typedef unsigned long OP_FULL_MODEL_ID;

#endif  /* __OPALTYPES__ */
