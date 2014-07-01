/*
 * asyncip_sl_1_sm_ip_test_private.h
 *
 * Code generation for model "asyncip_sl_1_sm_ip_test.mdl".
 *
 * Model version              : 1.426
 * Simulink Coder version : 8.1 (R2011b) 08-Jul-2011
 * C source code generated on : Wed May 28 12:53:42 2014
 *
 * Target selection: rtlab_rtmodel.tlc
 * Note: GRT includes extra infrastructure and instrumentation for prototyping
 * Embedded hardware selection: 32-bit Generic
 * Code generation objectives: Unspecified
 * Validation result: Not run
 */
#ifndef RTW_HEADER_asyncip_sl_1_sm_ip_test_private_h_
#define RTW_HEADER_asyncip_sl_1_sm_ip_test_private_h_
#include "rtwtypes.h"
#ifndef __RTWTYPES_H__
#error This file requires rtwtypes.h to be included
#else
#ifdef TMWTYPES_PREVIOUSLY_INCLUDED
#error This file requires rtwtypes.h to be included before tmwtypes.h
#else

/* Check for inclusion of an incorrect version of rtwtypes.h */
#ifndef RTWTYPES_ID_C08S16I32L32N32F1
#error This code was generated with a different "rtwtypes.h" than the file included
#endif                                 /* RTWTYPES_ID_C08S16I32L32N32F1 */
#endif                                 /* TMWTYPES_PREVIOUSLY_INCLUDED */
#endif                                 /* __RTWTYPES_H__ */

extern void sfun_send_async(SimStruct *rts);
extern void sfun_recv_async(SimStruct *rts);
extern void OP_SEND(SimStruct *rts);
extern void sfun_gen_async_ctrl(SimStruct *rts);

#endif                                 /* RTW_HEADER_asyncip_sl_1_sm_ip_test_private_h_ */
