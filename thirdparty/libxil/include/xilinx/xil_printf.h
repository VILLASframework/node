 #ifndef XIL_PRINTF_H
 #define XIL_PRINTF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "xil_types.h"

/*----------------------------------------------------*/
/* Use the following parameter passing structure to   */
/* make xil_printf re-entrant.                        */
/*----------------------------------------------------*/

struct params_s;


/*---------------------------------------------------*/
/* The purpose of this routine is to output data the */
/* same as the standard printf function without the  */
/* overhead most run-time libraries involve. Usually */
/* the printf brings in many kilobytes of code and   */
/* that is unacceptable in most embedded systems.    */
/*---------------------------------------------------*/

typedef char8* charptr;
typedef s32 (*func_ptr)(int c);

#define xil_printf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#define print(ptr)		printf("%s", ptr)

#define outbyte(byte)		putc(byte, stdout)
#define inbyte();		getc(stdin)

#ifdef __cplusplus
}
#endif

#endif	/* end of protection macro */
