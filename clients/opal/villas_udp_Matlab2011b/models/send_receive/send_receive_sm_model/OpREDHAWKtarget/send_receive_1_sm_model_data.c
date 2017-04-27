/*
 * send_receive_1_sm_model_data.c
 *
 * Code generation for model "send_receive_1_sm_model.mdl".
 *
 * Model version              : 1.453
 * Simulink Coder version : 8.1 (R2011b) 08-Jul-2011
 * C source code generated on : Thu Apr 27 18:29:02 2017
 *
 * Target selection: rtlab_rtmodel.tlc
 * Note: GRT includes extra infrastructure and instrumentation for prototyping
 * Embedded hardware selection: 32-bit Generic
 * Code generation objectives: Unspecified
 * Validation result: Not run
 */
#include "send_receive_1_sm_model.h"
#include "send_receive_1_sm_model_private.h"

/* Block parameters (auto storage) */
Parameters_send_receive_1_sm_model send_receive_1_sm_model_P = {
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S1>/S-Function1'
                                        */
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S1>/S-Function'
                                        */
  1.0,                                 /* Expression: 1
                                        * Referenced by: '<S2>/data ready 2 kHz'
                                        */
  10.0,                                /* Expression: 10
                                        * Referenced by: '<S2>/data ready 2 kHz'
                                        */
  1.0,                                 /* Expression: 1
                                        * Referenced by: '<S2>/data ready 2 kHz'
                                        */
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S2>/data ready 2 kHz'
                                        */

  /*  Expression: [1 2 3 4]
   * Referenced by: '<S2>/Constant'
   */
  { 1.0, 2.0, 3.0, 4.0 },
  5.0,                                 /* Expression: 5
                                        * Referenced by: '<S2>/Pulse Generator1'
                                        */
  2000.0,                              /* Computed Parameter: PulseGenerator1_Period
                                        * Referenced by: '<S2>/Pulse Generator1'
                                        */
  600.0,                               /* Computed Parameter: PulseGenerator1_Duty
                                        * Referenced by: '<S2>/Pulse Generator1'
                                        */
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S2>/Pulse Generator1'
                                        */

  /*  Computed Parameter: SFunction2_P1_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 1.0 },
  1.0,                                 /* Expression: ctl_id
                                        * Referenced by: '<S5>/S-Function2'
                                        */

  /*  Computed Parameter: SFunction2_P2_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 1.0 },
  1.0,                                 /* Expression: send_id
                                        * Referenced by: '<S5>/S-Function2'
                                        */

  /*  Computed Parameter: SFunction2_P3_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 1.0 },
  3.0,                                 /* Expression: mode
                                        * Referenced by: '<S5>/S-Function2'
                                        */

  /*  Computed Parameter: SFunction2_P4_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 1.0 },
  1.0,                                 /* Expression: fp1
                                        * Referenced by: '<S5>/S-Function2'
                                        */

  /*  Computed Parameter: SFunction2_P5_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 1.0 },
  2.0,                                 /* Expression: fp2
                                        * Referenced by: '<S5>/S-Function2'
                                        */

  /*  Computed Parameter: SFunction2_P6_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 1.0 },
  3.0,                                 /* Expression: fp3
                                        * Referenced by: '<S5>/S-Function2'
                                        */

  /*  Computed Parameter: SFunction2_P7_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 1.0 },
  4.0,                                 /* Expression: fp4
                                        * Referenced by: '<S5>/S-Function2'
                                        */

  /*  Computed Parameter: SFunction2_P8_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 1.0 },
  5.0,                                 /* Expression: fp5
                                        * Referenced by: '<S5>/S-Function2'
                                        */

  /*  Computed Parameter: SFunction2_P9_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 7.0 },

  /*  Computed Parameter: SFunction2_P9
   * Referenced by: '<S5>/S-Function2'
   */
  { 115.0, 116.0, 114.0, 105.0, 110.0, 103.0, 49.0 },

  /*  Computed Parameter: SFunction2_P10_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 7.0 },

  /*  Computed Parameter: SFunction2_P10
   * Referenced by: '<S5>/S-Function2'
   */
  { 115.0, 116.0, 114.0, 105.0, 110.0, 103.0, 50.0 },

  /*  Computed Parameter: SFunction2_P11_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 7.0 },

  /*  Computed Parameter: SFunction2_P11
   * Referenced by: '<S5>/S-Function2'
   */
  { 115.0, 116.0, 114.0, 105.0, 110.0, 103.0, 51.0 },

  /*  Computed Parameter: SFunction2_P12_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 7.0 },

  /*  Computed Parameter: SFunction2_P12
   * Referenced by: '<S5>/S-Function2'
   */
  { 115.0, 116.0, 114.0, 105.0, 110.0, 103.0, 52.0 },

  /*  Computed Parameter: SFunction2_P13_Size
   * Referenced by: '<S5>/S-Function2'
   */
  { 1.0, 7.0 },

  /*  Computed Parameter: SFunction2_P13
   * Referenced by: '<S5>/S-Function2'
   */
  { 115.0, 116.0, 114.0, 105.0, 110.0, 103.0, 53.0 },
  2.0,                                 /* Expression: 2
                                        * Referenced by: '<S2>/timeout'
                                        */

  /*  Computed Parameter: SFunction1_P1_Size
   * Referenced by: '<S3>/S-Function1'
   */
  { 1.0, 1.0 },
  1.0,                                 /* Expression: ctl_id
                                        * Referenced by: '<S3>/S-Function1'
                                        */

  /*  Computed Parameter: SFunction1_P2_Size
   * Referenced by: '<S3>/S-Function1'
   */
  { 1.0, 1.0 },
  1.0,                                 /* Expression: recv_id
                                        * Referenced by: '<S3>/S-Function1'
                                        */

  /*  Computed Parameter: SFunction1_P3_Size
   * Referenced by: '<S3>/S-Function1'
   */
  { 1.0, 1.0 },
  1.0,                                 /* Expression: fp1
                                        * Referenced by: '<S3>/S-Function1'
                                        */

  /*  Computed Parameter: SFunction1_P4_Size
   * Referenced by: '<S3>/S-Function1'
   */
  { 1.0, 1.0 },
  2.0,                                 /* Expression: fp2
                                        * Referenced by: '<S3>/S-Function1'
                                        */

  /*  Computed Parameter: SFunction1_P5_Size
   * Referenced by: '<S3>/S-Function1'
   */
  { 1.0, 1.0 },
  3.0,                                 /* Expression: fp3
                                        * Referenced by: '<S3>/S-Function1'
                                        */

  /*  Computed Parameter: SFunction1_P6_Size
   * Referenced by: '<S3>/S-Function1'
   */
  { 1.0, 1.0 },
  4.0,                                 /* Expression: fp4
                                        * Referenced by: '<S3>/S-Function1'
                                        */

  /*  Computed Parameter: SFunction1_P7_Size
   * Referenced by: '<S3>/S-Function1'
   */
  { 1.0, 1.0 },
  5.0,                                 /* Expression: fp5
                                        * Referenced by: '<S3>/S-Function1'
                                        */

  /*  Computed Parameter: SFunction1_P8_Size
   * Referenced by: '<S3>/S-Function1'
   */
  { 1.0, 7.0 },

  /*  Computed Parameter: SFunction1_P8
   * Referenced by: '<S3>/S-Function1'
   */
  { 115.0, 116.0, 114.0, 105.0, 110.0, 103.0, 49.0 },

  /*  Computed Parameter: SFunction1_P9_Size
   * Referenced by: '<S3>/S-Function1'
   */
  { 1.0, 7.0 },

  /*  Computed Parameter: SFunction1_P9
   * Referenced by: '<S3>/S-Function1'
   */
  { 115.0, 116.0, 114.0, 105.0, 110.0, 103.0, 50.0 },

  /*  Computed Parameter: SFunction1_P10_Size
   * Referenced by: '<S3>/S-Function1'
   */
  { 1.0, 7.0 },

  /*  Computed Parameter: SFunction1_P10
   * Referenced by: '<S3>/S-Function1'
   */
  { 115.0, 116.0, 114.0, 105.0, 110.0, 103.0, 51.0 },

  /*  Computed Parameter: SFunction1_P11_Size
   * Referenced by: '<S3>/S-Function1'
   */
  { 1.0, 7.0 },

  /*  Computed Parameter: SFunction1_P11
   * Referenced by: '<S3>/S-Function1'
   */
  { 115.0, 116.0, 114.0, 105.0, 110.0, 103.0, 52.0 },

  /*  Computed Parameter: SFunction1_P12_Size
   * Referenced by: '<S3>/S-Function1'
   */
  { 1.0, 7.0 },

  /*  Computed Parameter: SFunction1_P12
   * Referenced by: '<S3>/S-Function1'
   */
  { 115.0, 116.0, 114.0, 105.0, 110.0, 103.0, 53.0 },

  /*  Computed Parameter: SFunction_P1_Size
   * Referenced by: '<S7>/S-Function'
   */
  { 1.0, 1.0 },
  1.0,                                 /* Expression: Acqu_group
                                        * Referenced by: '<S7>/S-Function'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P1_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  1.0,                                 /* Expression: ctl_id
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P2_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  1.0,                                 /* Expression: proto
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P3_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  12000.0,                             /* Expression: ip_port_remote
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P4_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  12001.0,                             /* Expression: ip_port_local
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P5_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P6_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P7_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P8_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P9_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P10_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P11_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P12_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P13_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  0.0,                                 /* Expression: 0
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */

  /*  Computed Parameter: OpIPSocketCtrl1_P14_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 14.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P14
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 49.0, 51.0, 52.0, 46.0, 49.0, 51.0, 48.0, 46.0, 49.0, 54.0, 57.0, 46.0, 51.0,
    49.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P15_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 7.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P15
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 48.0, 46.0, 48.0, 46.0, 48.0, 46.0, 48.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P16_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 0.0, 0.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P17_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 0.0, 0.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P18_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 0.0, 0.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P19_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 0.0, 0.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P20_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 0.0, 0.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P21_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 0.0, 0.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P22_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 0.0, 0.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P23_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 0.0, 0.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P24_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 0.0, 0.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P25_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 0.0, 0.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P26_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 7.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P26
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 65.0, 115.0, 121.0, 110.0, 99.0, 73.0, 80.0 },

  /*  Computed Parameter: OpIPSocketCtrl1_P27_Size
   * Referenced by: '<S2>/OpIPSocketCtrl1'
   */
  { 1.0, 1.0 },
  0.0                                  /* Expression: 0
                                        * Referenced by: '<S2>/OpIPSocketCtrl1'
                                        */
};
