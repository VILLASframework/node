/*
 * send_receive_1_sm_model.c
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

/* Block signals (auto storage) */
BlockIO_send_receive_1_sm_model send_receive_1_sm_model_B;

/* Block states (auto storage) */
D_Work_send_receive_1_sm_model send_receive_1_sm_model_DWork;

/* Real-time model */
RT_MODEL_send_receive_1_sm_model send_receive_1_sm_model_M_;
RT_MODEL_send_receive_1_sm_model *const send_receive_1_sm_model_M =
  &send_receive_1_sm_model_M_;

/* Model output function */
static void send_receive_1_sm_model_output(int_T tid)
{
  /* Memory: '<S1>/S-Function' */
  send_receive_1_sm_model_B.SFunction =
    send_receive_1_sm_model_DWork.SFunction_PreviousInput;

  /* Sum: '<S1>/Sum' incorporates:
   *  Constant: '<S1>/S-Function1'
   */
  send_receive_1_sm_model_B.Sum = send_receive_1_sm_model_P.SFunction1_Value +
    send_receive_1_sm_model_B.SFunction;

  /* Stop: '<S1>/Stop Simulation' */
  if (send_receive_1_sm_model_B.Sum != 0.0) {
    rtmSetStopRequested(send_receive_1_sm_model_M, 1);
  }

  /* End of Stop: '<S1>/Stop Simulation' */

  /* DiscretePulseGenerator: '<S2>/data ready 2 kHz' */
  send_receive_1_sm_model_B.dataready2kHz = ((real_T)
    send_receive_1_sm_model_DWork.clockTickCounter <
    send_receive_1_sm_model_P.dataready2kHz_Duty) &&
    (send_receive_1_sm_model_DWork.clockTickCounter >= 0) ?
    send_receive_1_sm_model_P.dataready2kHz_Amp : 0.0;
  if ((real_T)send_receive_1_sm_model_DWork.clockTickCounter >=
      send_receive_1_sm_model_P.dataready2kHz_Period - 1.0) {
    send_receive_1_sm_model_DWork.clockTickCounter = 0;
  } else {
    send_receive_1_sm_model_DWork.clockTickCounter =
      send_receive_1_sm_model_DWork.clockTickCounter + 1;
  }

  /* End of DiscretePulseGenerator: '<S2>/data ready 2 kHz' */

  /* DiscretePulseGenerator: '<S2>/Pulse Generator1' */
  send_receive_1_sm_model_B.PulseGenerator1 = ((real_T)
    send_receive_1_sm_model_DWork.clockTickCounter_h <
    send_receive_1_sm_model_P.PulseGenerator1_Duty) &&
    (send_receive_1_sm_model_DWork.clockTickCounter_h >= 0) ?
    send_receive_1_sm_model_P.PulseGenerator1_Amp : 0.0;
  if ((real_T)send_receive_1_sm_model_DWork.clockTickCounter_h >=
      send_receive_1_sm_model_P.PulseGenerator1_Period - 1.0) {
    send_receive_1_sm_model_DWork.clockTickCounter_h = 0;
  } else {
    send_receive_1_sm_model_DWork.clockTickCounter_h =
      send_receive_1_sm_model_DWork.clockTickCounter_h + 1;
  }

  /* End of DiscretePulseGenerator: '<S2>/Pulse Generator1' */

  /* Level2 S-Function Block: '<S5>/S-Function2' (sfun_send_async) */
  {
    SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[0];
    sfcnOutputs(rts, 0);
  }

  /* Level2 S-Function Block: '<S3>/S-Function1' (sfun_recv_async) */
  {
    SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[1];
    sfcnOutputs(rts, 0);
  }

  /* Level2 S-Function Block: '<S7>/S-Function' (OP_SEND) */
  {
    SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[2];
    sfcnOutputs(rts, 0);
  }

  /* Level2 S-Function Block: '<S2>/OpIPSocketCtrl1' (sfun_gen_async_ctrl) */
  {
    SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[3];
    sfcnOutputs(rts, 0);
  }

  /* tid is required for a uniform function interface.
   * Argument tid is not used in the function. */
  UNUSED_PARAMETER(tid);
}

/* Model update function */
static void send_receive_1_sm_model_update(int_T tid)
{
  /* Update for Memory: '<S1>/S-Function' */
  send_receive_1_sm_model_DWork.SFunction_PreviousInput =
    send_receive_1_sm_model_B.Sum;

  /* Update absolute time for base rate */
  /* The "clockTick0" counts the number of times the code of this task has
   * been executed. The absolute time is the multiplication of "clockTick0"
   * and "Timing.stepSize0". Size of "clockTick0" ensures timer will not
   * overflow during the application lifespan selected.
   * Timer of this task consists of two 32 bit unsigned integers.
   * The two integers represent the low bits Timing.clockTick0 and the high bits
   * Timing.clockTickH0. When the low bit overflows to 0, the high bits increment.
   */
  if (!(++send_receive_1_sm_model_M->Timing.clockTick0)) {
    ++send_receive_1_sm_model_M->Timing.clockTickH0;
  }

  send_receive_1_sm_model_M->Timing.t[0] =
    send_receive_1_sm_model_M->Timing.clockTick0 *
    send_receive_1_sm_model_M->Timing.stepSize0 +
    send_receive_1_sm_model_M->Timing.clockTickH0 *
    send_receive_1_sm_model_M->Timing.stepSize0 * 4294967296.0;

  /* tid is required for a uniform function interface.
   * Argument tid is not used in the function. */
  UNUSED_PARAMETER(tid);
}

/* Model initialize function */
void send_receive_1_sm_model_initialize(boolean_T firstTime)
{
  (void)firstTime;

  /* Registration code */

  /* initialize non-finites */
  rt_InitInfAndNaN(sizeof(real_T));

  /* initialize real-time model */
  (void) memset((void *)send_receive_1_sm_model_M, 0,
                sizeof(RT_MODEL_send_receive_1_sm_model));
  rtsiSetSolverName(&send_receive_1_sm_model_M->solverInfo,"FixedStepDiscrete");
  send_receive_1_sm_model_M->solverInfoPtr =
    (&send_receive_1_sm_model_M->solverInfo);

  /* Initialize timing info */
  {
    int_T *mdlTsMap = send_receive_1_sm_model_M->Timing.sampleTimeTaskIDArray;
    mdlTsMap[0] = 0;
    send_receive_1_sm_model_M->Timing.sampleTimeTaskIDPtr = (&mdlTsMap[0]);
    send_receive_1_sm_model_M->Timing.sampleTimes =
      (&send_receive_1_sm_model_M->Timing.sampleTimesArray[0]);
    send_receive_1_sm_model_M->Timing.offsetTimes =
      (&send_receive_1_sm_model_M->Timing.offsetTimesArray[0]);

    /* task periods */
    send_receive_1_sm_model_M->Timing.sampleTimes[0] = (5.0E-5);

    /* task offsets */
    send_receive_1_sm_model_M->Timing.offsetTimes[0] = (0.0);
  }

  rtmSetTPtr(send_receive_1_sm_model_M,
             &send_receive_1_sm_model_M->Timing.tArray[0]);

  {
    int_T *mdlSampleHits = send_receive_1_sm_model_M->Timing.sampleHitArray;
    mdlSampleHits[0] = 1;
    send_receive_1_sm_model_M->Timing.sampleHits = (&mdlSampleHits[0]);
  }

  rtmSetTFinal(send_receive_1_sm_model_M, -1);
  send_receive_1_sm_model_M->Timing.stepSize0 = 5.0E-5;

  /* Setup for data logging */
  {
    static RTWLogInfo rt_DataLoggingInfo;
    send_receive_1_sm_model_M->rtwLogInfo = &rt_DataLoggingInfo;
  }

  /* Setup for data logging */
  {
    rtliSetLogXSignalInfo(send_receive_1_sm_model_M->rtwLogInfo, (NULL));
    rtliSetLogXSignalPtrs(send_receive_1_sm_model_M->rtwLogInfo, (NULL));
    rtliSetLogT(send_receive_1_sm_model_M->rtwLogInfo, "");
    rtliSetLogX(send_receive_1_sm_model_M->rtwLogInfo, "");
    rtliSetLogXFinal(send_receive_1_sm_model_M->rtwLogInfo, "");
    rtliSetSigLog(send_receive_1_sm_model_M->rtwLogInfo, "");
    rtliSetLogVarNameModifier(send_receive_1_sm_model_M->rtwLogInfo, "rt_");
    rtliSetLogFormat(send_receive_1_sm_model_M->rtwLogInfo, 0);
    rtliSetLogMaxRows(send_receive_1_sm_model_M->rtwLogInfo, 0);
    rtliSetLogDecimation(send_receive_1_sm_model_M->rtwLogInfo, 1);
    rtliSetLogY(send_receive_1_sm_model_M->rtwLogInfo, "");
    rtliSetLogYSignalInfo(send_receive_1_sm_model_M->rtwLogInfo, (NULL));
    rtliSetLogYSignalPtrs(send_receive_1_sm_model_M->rtwLogInfo, (NULL));
  }

  send_receive_1_sm_model_M->solverInfoPtr =
    (&send_receive_1_sm_model_M->solverInfo);
  send_receive_1_sm_model_M->Timing.stepSize = (5.0E-5);
  rtsiSetFixedStepSize(&send_receive_1_sm_model_M->solverInfo, 5.0E-5);
  rtsiSetSolverMode(&send_receive_1_sm_model_M->solverInfo,
                    SOLVER_MODE_SINGLETASKING);

  /* block I/O */
  send_receive_1_sm_model_M->ModelData.blockIO = ((void *)
    &send_receive_1_sm_model_B);

  {
    int_T i;
    for (i = 0; i < 5; i++) {
      send_receive_1_sm_model_B.SFunction1_o3[i] = 0.0;
    }

    send_receive_1_sm_model_B.SFunction = 0.0;
    send_receive_1_sm_model_B.Sum = 0.0;
    send_receive_1_sm_model_B.dataready2kHz = 0.0;
    send_receive_1_sm_model_B.PulseGenerator1 = 0.0;
    send_receive_1_sm_model_B.SFunction2 = 0.0;
    send_receive_1_sm_model_B.SFunction1_o1 = 0.0;
    send_receive_1_sm_model_B.SFunction1_o2 = 0.0;
  }

  /* parameters */
  send_receive_1_sm_model_M->ModelData.defaultParam = ((real_T *)
    &send_receive_1_sm_model_P);

  /* states (dwork) */
  send_receive_1_sm_model_M->Work.dwork = ((void *)
    &send_receive_1_sm_model_DWork);
  (void) memset((void *)&send_receive_1_sm_model_DWork, 0,
                sizeof(D_Work_send_receive_1_sm_model));
  send_receive_1_sm_model_DWork.SFunction_PreviousInput = 0.0;

  /* child S-Function registration */
  {
    RTWSfcnInfo *sfcnInfo = &send_receive_1_sm_model_M->NonInlinedSFcns.sfcnInfo;
    send_receive_1_sm_model_M->sfcnInfo = (sfcnInfo);
    rtssSetErrorStatusPtr(sfcnInfo, (&rtmGetErrorStatus
      (send_receive_1_sm_model_M)));
    rtssSetNumRootSampTimesPtr(sfcnInfo,
      &send_receive_1_sm_model_M->Sizes.numSampTimes);
    send_receive_1_sm_model_M->NonInlinedSFcns.taskTimePtrs[0] = &(rtmGetTPtr
      (send_receive_1_sm_model_M)[0]);
    rtssSetTPtrPtr(sfcnInfo,
                   send_receive_1_sm_model_M->NonInlinedSFcns.taskTimePtrs);
    rtssSetTStartPtr(sfcnInfo, &rtmGetTStart(send_receive_1_sm_model_M));
    rtssSetTFinalPtr(sfcnInfo, &rtmGetTFinal(send_receive_1_sm_model_M));
    rtssSetTimeOfLastOutputPtr(sfcnInfo, &rtmGetTimeOfLastOutput
      (send_receive_1_sm_model_M));
    rtssSetStepSizePtr(sfcnInfo, &send_receive_1_sm_model_M->Timing.stepSize);
    rtssSetStopRequestedPtr(sfcnInfo, &rtmGetStopRequested
      (send_receive_1_sm_model_M));
    rtssSetDerivCacheNeedsResetPtr(sfcnInfo,
      &send_receive_1_sm_model_M->ModelData.derivCacheNeedsReset);
    rtssSetZCCacheNeedsResetPtr(sfcnInfo,
      &send_receive_1_sm_model_M->ModelData.zCCacheNeedsReset);
    rtssSetBlkStateChangePtr(sfcnInfo,
      &send_receive_1_sm_model_M->ModelData.blkStateChange);
    rtssSetSampleHitsPtr(sfcnInfo, &send_receive_1_sm_model_M->Timing.sampleHits);
    rtssSetPerTaskSampleHitsPtr(sfcnInfo,
      &send_receive_1_sm_model_M->Timing.perTaskSampleHits);
    rtssSetSimModePtr(sfcnInfo, &send_receive_1_sm_model_M->simMode);
    rtssSetSolverInfoPtr(sfcnInfo, &send_receive_1_sm_model_M->solverInfoPtr);
  }

  send_receive_1_sm_model_M->Sizes.numSFcns = (4);

  /* register each child */
  {
    (void) memset((void *)
                  &send_receive_1_sm_model_M->NonInlinedSFcns.childSFunctions[0],
                  0,
                  4*sizeof(SimStruct));
    send_receive_1_sm_model_M->childSfunctions =
      (&send_receive_1_sm_model_M->NonInlinedSFcns.childSFunctionPtrs[0]);
    send_receive_1_sm_model_M->childSfunctions[0] =
      (&send_receive_1_sm_model_M->NonInlinedSFcns.childSFunctions[0]);
    send_receive_1_sm_model_M->childSfunctions[1] =
      (&send_receive_1_sm_model_M->NonInlinedSFcns.childSFunctions[1]);
    send_receive_1_sm_model_M->childSfunctions[2] =
      (&send_receive_1_sm_model_M->NonInlinedSFcns.childSFunctions[2]);
    send_receive_1_sm_model_M->childSfunctions[3] =
      (&send_receive_1_sm_model_M->NonInlinedSFcns.childSFunctions[3]);

    /* Level2 S-Function Block: send_receive_1_sm_model/<S5>/S-Function2 (sfun_send_async) */
    {
      SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[0];

      /* timing info */
      time_T *sfcnPeriod =
        send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn0.sfcnPeriod;
      time_T *sfcnOffset =
        send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn0.sfcnOffset;
      int_T *sfcnTsMap =
        send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn0.sfcnTsMap;
      (void) memset((void*)sfcnPeriod, 0,
                    sizeof(time_T)*1);
      (void) memset((void*)sfcnOffset, 0,
                    sizeof(time_T)*1);
      ssSetSampleTimePtr(rts, &sfcnPeriod[0]);
      ssSetOffsetTimePtr(rts, &sfcnOffset[0]);
      ssSetSampleTimeTaskIDPtr(rts, sfcnTsMap);

      /* Set up the mdlInfo pointer */
      {
        ssSetBlkInfo2Ptr(rts,
                         &send_receive_1_sm_model_M->NonInlinedSFcns.blkInfo2[0]);
      }

      ssSetRTWSfcnInfo(rts, send_receive_1_sm_model_M->sfcnInfo);

      /* Allocate memory of model methods 2 */
      {
        ssSetModelMethods2(rts,
                           &send_receive_1_sm_model_M->NonInlinedSFcns.methods2
                           [0]);
      }

      /* Allocate memory of model methods 3 */
      {
        ssSetModelMethods3(rts,
                           &send_receive_1_sm_model_M->NonInlinedSFcns.methods3
                           [0]);
      }

      /* Allocate memory for states auxilliary information */
      {
        ssSetStatesInfo2(rts,
                         &send_receive_1_sm_model_M->
                         NonInlinedSFcns.statesInfo2[0]);
      }

      /* inputs */
      {
        _ssSetNumInputPorts(rts, 2);
        ssSetPortInfoForInputs(rts,
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn0.inputPortInfo[0]);

        /* port 0 */
        {
          real_T const **sfcnUPtrs = (real_T const **)
            &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn0.UPtrs0;
          sfcnUPtrs[0] = &send_receive_1_sm_model_B.dataready2kHz;
          ssSetInputPortSignalPtrs(rts, 0, (InputPtrsType)&sfcnUPtrs[0]);
          _ssSetInputPortNumDimensions(rts, 0, 1);
          ssSetInputPortWidth(rts, 0, 1);
        }

        /* port 1 */
        {
          real_T const **sfcnUPtrs = (real_T const **)
            &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn0.UPtrs1;
          sfcnUPtrs[0] = send_receive_1_sm_model_P.Constant_Value;
          sfcnUPtrs[1] = &send_receive_1_sm_model_P.Constant_Value[1];
          sfcnUPtrs[2] = &send_receive_1_sm_model_P.Constant_Value[2];
          sfcnUPtrs[3] = &send_receive_1_sm_model_P.Constant_Value[3];
          sfcnUPtrs[4] = &send_receive_1_sm_model_B.PulseGenerator1;
          ssSetInputPortSignalPtrs(rts, 1, (InputPtrsType)&sfcnUPtrs[0]);
          _ssSetInputPortNumDimensions(rts, 1, 1);
          ssSetInputPortWidth(rts, 1, 5);
        }
      }

      /* outputs */
      {
        ssSetPortInfoForOutputs(rts,
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn0.outputPortInfo[0]);
        _ssSetNumOutputPorts(rts, 1);

        /* port 0 */
        {
          _ssSetOutputPortNumDimensions(rts, 0, 1);
          ssSetOutputPortWidth(rts, 0, 1);
          ssSetOutputPortSignal(rts, 0, ((real_T *)
            &send_receive_1_sm_model_B.SFunction2));
        }
      }

      /* path info */
      ssSetModelName(rts, "S-Function2");
      ssSetPath(rts,
                "send_receive_1_sm_model/sm_model/send message 1/S-Function2");
      ssSetRTModel(rts,send_receive_1_sm_model_M);
      ssSetParentSS(rts, (NULL));
      ssSetRootSS(rts, rts);
      ssSetVersion(rts, SIMSTRUCT_VERSION_LEVEL2);

      /* parameters */
      {
        mxArray **sfcnParams = (mxArray **)
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn0.params;
        ssSetSFcnParamsCount(rts, 13);
        ssSetSFcnParamsPtr(rts, &sfcnParams[0]);
        ssSetSFcnParam(rts, 0, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P1_Size);
        ssSetSFcnParam(rts, 1, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P2_Size);
        ssSetSFcnParam(rts, 2, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P3_Size);
        ssSetSFcnParam(rts, 3, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P4_Size);
        ssSetSFcnParam(rts, 4, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P5_Size);
        ssSetSFcnParam(rts, 5, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P6_Size);
        ssSetSFcnParam(rts, 6, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P7_Size);
        ssSetSFcnParam(rts, 7, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P8_Size);
        ssSetSFcnParam(rts, 8, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P9_Size);
        ssSetSFcnParam(rts, 9, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P10_Size);
        ssSetSFcnParam(rts, 10, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P11_Size);
        ssSetSFcnParam(rts, 11, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P12_Size);
        ssSetSFcnParam(rts, 12, (mxArray*)
                       send_receive_1_sm_model_P.SFunction2_P13_Size);
      }

      /* work vectors */
      ssSetPWork(rts, (void **) &send_receive_1_sm_model_DWork.SFunction2_PWORK);

      {
        struct _ssDWorkRecord *dWorkRecord = (struct _ssDWorkRecord *)
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn0.dWork;
        struct _ssDWorkAuxRecord *dWorkAuxRecord = (struct _ssDWorkAuxRecord *)
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn0.dWorkAux;
        ssSetSFcnDWork(rts, dWorkRecord);
        ssSetSFcnDWorkAux(rts, dWorkAuxRecord);
        _ssSetNumDWork(rts, 1);

        /* PWORK */
        ssSetDWorkWidth(rts, 0, 1);
        ssSetDWorkDataType(rts, 0,SS_POINTER);
        ssSetDWorkComplexSignal(rts, 0, 0);
        ssSetDWork(rts, 0, &send_receive_1_sm_model_DWork.SFunction2_PWORK);
      }

      /* registration */
      sfun_send_async(rts);
      sfcnInitializeSizes(rts);
      sfcnInitializeSampleTimes(rts);

      /* adjust sample time */
      ssSetSampleTime(rts, 0, 5.0E-5);
      ssSetOffsetTime(rts, 0, 0.0);
      sfcnTsMap[0] = 0;

      /* set compiled values of dynamic vector attributes */
      ssSetInputPortWidth(rts, 1, 5);
      ssSetInputPortDataType(rts, 1, SS_DOUBLE);
      ssSetInputPortComplexSignal(rts, 1, 0);
      ssSetInputPortFrameData(rts, 1, 0);
      ssSetNumNonsampledZCs(rts, 0);

      /* Update connectivity flags for each port */
      _ssSetInputPortConnected(rts, 0, 1);
      _ssSetInputPortConnected(rts, 1, 1);
      _ssSetOutputPortConnected(rts, 0, 1);
      _ssSetOutputPortBeingMerged(rts, 0, 0);

      /* Update the BufferDstPort flags for each input port */
      ssSetInputPortBufferDstPort(rts, 0, -1);
      ssSetInputPortBufferDstPort(rts, 1, -1);
    }

    /* Level2 S-Function Block: send_receive_1_sm_model/<S3>/S-Function1 (sfun_recv_async) */
    {
      SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[1];

      /* timing info */
      time_T *sfcnPeriod =
        send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn1.sfcnPeriod;
      time_T *sfcnOffset =
        send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn1.sfcnOffset;
      int_T *sfcnTsMap =
        send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn1.sfcnTsMap;
      (void) memset((void*)sfcnPeriod, 0,
                    sizeof(time_T)*1);
      (void) memset((void*)sfcnOffset, 0,
                    sizeof(time_T)*1);
      ssSetSampleTimePtr(rts, &sfcnPeriod[0]);
      ssSetOffsetTimePtr(rts, &sfcnOffset[0]);
      ssSetSampleTimeTaskIDPtr(rts, sfcnTsMap);

      /* Set up the mdlInfo pointer */
      {
        ssSetBlkInfo2Ptr(rts,
                         &send_receive_1_sm_model_M->NonInlinedSFcns.blkInfo2[1]);
      }

      ssSetRTWSfcnInfo(rts, send_receive_1_sm_model_M->sfcnInfo);

      /* Allocate memory of model methods 2 */
      {
        ssSetModelMethods2(rts,
                           &send_receive_1_sm_model_M->NonInlinedSFcns.methods2
                           [1]);
      }

      /* Allocate memory of model methods 3 */
      {
        ssSetModelMethods3(rts,
                           &send_receive_1_sm_model_M->NonInlinedSFcns.methods3
                           [1]);
      }

      /* Allocate memory for states auxilliary information */
      {
        ssSetStatesInfo2(rts,
                         &send_receive_1_sm_model_M->
                         NonInlinedSFcns.statesInfo2[1]);
      }

      /* inputs */
      {
        _ssSetNumInputPorts(rts, 1);
        ssSetPortInfoForInputs(rts,
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn1.inputPortInfo[0]);

        /* port 0 */
        {
          real_T const **sfcnUPtrs = (real_T const **)
            &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn1.UPtrs0;
          sfcnUPtrs[0] = &send_receive_1_sm_model_P.timeout_Value;
          ssSetInputPortSignalPtrs(rts, 0, (InputPtrsType)&sfcnUPtrs[0]);
          _ssSetInputPortNumDimensions(rts, 0, 1);
          ssSetInputPortWidth(rts, 0, 1);
        }
      }

      /* outputs */
      {
        ssSetPortInfoForOutputs(rts,
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn1.outputPortInfo[0]);
        _ssSetNumOutputPorts(rts, 3);

        /* port 0 */
        {
          _ssSetOutputPortNumDimensions(rts, 0, 1);
          ssSetOutputPortWidth(rts, 0, 1);
          ssSetOutputPortSignal(rts, 0, ((real_T *)
            &send_receive_1_sm_model_B.SFunction1_o1));
        }

        /* port 1 */
        {
          _ssSetOutputPortNumDimensions(rts, 1, 1);
          ssSetOutputPortWidth(rts, 1, 1);
          ssSetOutputPortSignal(rts, 1, ((real_T *)
            &send_receive_1_sm_model_B.SFunction1_o2));
        }

        /* port 2 */
        {
          _ssSetOutputPortNumDimensions(rts, 2, 1);
          ssSetOutputPortWidth(rts, 2, 5);
          ssSetOutputPortSignal(rts, 2, ((real_T *)
            send_receive_1_sm_model_B.SFunction1_o3));
        }
      }

      /* path info */
      ssSetModelName(rts, "S-Function1");
      ssSetPath(rts,
                "send_receive_1_sm_model/sm_model/receive message 1/S-Function1");
      ssSetRTModel(rts,send_receive_1_sm_model_M);
      ssSetParentSS(rts, (NULL));
      ssSetRootSS(rts, rts);
      ssSetVersion(rts, SIMSTRUCT_VERSION_LEVEL2);

      /* parameters */
      {
        mxArray **sfcnParams = (mxArray **)
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn1.params;
        ssSetSFcnParamsCount(rts, 12);
        ssSetSFcnParamsPtr(rts, &sfcnParams[0]);
        ssSetSFcnParam(rts, 0, (mxArray*)
                       send_receive_1_sm_model_P.SFunction1_P1_Size);
        ssSetSFcnParam(rts, 1, (mxArray*)
                       send_receive_1_sm_model_P.SFunction1_P2_Size);
        ssSetSFcnParam(rts, 2, (mxArray*)
                       send_receive_1_sm_model_P.SFunction1_P3_Size);
        ssSetSFcnParam(rts, 3, (mxArray*)
                       send_receive_1_sm_model_P.SFunction1_P4_Size);
        ssSetSFcnParam(rts, 4, (mxArray*)
                       send_receive_1_sm_model_P.SFunction1_P5_Size);
        ssSetSFcnParam(rts, 5, (mxArray*)
                       send_receive_1_sm_model_P.SFunction1_P6_Size);
        ssSetSFcnParam(rts, 6, (mxArray*)
                       send_receive_1_sm_model_P.SFunction1_P7_Size);
        ssSetSFcnParam(rts, 7, (mxArray*)
                       send_receive_1_sm_model_P.SFunction1_P8_Size);
        ssSetSFcnParam(rts, 8, (mxArray*)
                       send_receive_1_sm_model_P.SFunction1_P9_Size);
        ssSetSFcnParam(rts, 9, (mxArray*)
                       send_receive_1_sm_model_P.SFunction1_P10_Size);
        ssSetSFcnParam(rts, 10, (mxArray*)
                       send_receive_1_sm_model_P.SFunction1_P11_Size);
        ssSetSFcnParam(rts, 11, (mxArray*)
                       send_receive_1_sm_model_P.SFunction1_P12_Size);
      }

      /* work vectors */
      ssSetPWork(rts, (void **) &send_receive_1_sm_model_DWork.SFunction1_PWORK);

      {
        struct _ssDWorkRecord *dWorkRecord = (struct _ssDWorkRecord *)
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn1.dWork;
        struct _ssDWorkAuxRecord *dWorkAuxRecord = (struct _ssDWorkAuxRecord *)
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn1.dWorkAux;
        ssSetSFcnDWork(rts, dWorkRecord);
        ssSetSFcnDWorkAux(rts, dWorkAuxRecord);
        _ssSetNumDWork(rts, 1);

        /* PWORK */
        ssSetDWorkWidth(rts, 0, 1);
        ssSetDWorkDataType(rts, 0,SS_POINTER);
        ssSetDWorkComplexSignal(rts, 0, 0);
        ssSetDWork(rts, 0, &send_receive_1_sm_model_DWork.SFunction1_PWORK);
      }

      /* registration */
      sfun_recv_async(rts);
      sfcnInitializeSizes(rts);
      sfcnInitializeSampleTimes(rts);

      /* adjust sample time */
      ssSetSampleTime(rts, 0, 5.0E-5);
      ssSetOffsetTime(rts, 0, 0.0);
      sfcnTsMap[0] = 0;

      /* set compiled values of dynamic vector attributes */
      ssSetOutputPortWidth(rts, 2, 5);
      ssSetOutputPortDataType(rts, 2, SS_DOUBLE);
      ssSetOutputPortComplexSignal(rts, 2, 0);
      ssSetOutputPortFrameData(rts, 2, 0);
      ssSetNumNonsampledZCs(rts, 0);

      /* Update connectivity flags for each port */
      _ssSetInputPortConnected(rts, 0, 1);
      _ssSetOutputPortConnected(rts, 0, 1);
      _ssSetOutputPortConnected(rts, 1, 1);
      _ssSetOutputPortConnected(rts, 2, 1);
      _ssSetOutputPortBeingMerged(rts, 0, 0);
      _ssSetOutputPortBeingMerged(rts, 1, 0);
      _ssSetOutputPortBeingMerged(rts, 2, 0);

      /* Update the BufferDstPort flags for each input port */
      ssSetInputPortBufferDstPort(rts, 0, -1);
    }

    /* Level2 S-Function Block: send_receive_1_sm_model/<S7>/S-Function (OP_SEND) */
    {
      SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[2];

      /* timing info */
      time_T *sfcnPeriod =
        send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn2.sfcnPeriod;
      time_T *sfcnOffset =
        send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn2.sfcnOffset;
      int_T *sfcnTsMap =
        send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn2.sfcnTsMap;
      (void) memset((void*)sfcnPeriod, 0,
                    sizeof(time_T)*1);
      (void) memset((void*)sfcnOffset, 0,
                    sizeof(time_T)*1);
      ssSetSampleTimePtr(rts, &sfcnPeriod[0]);
      ssSetOffsetTimePtr(rts, &sfcnOffset[0]);
      ssSetSampleTimeTaskIDPtr(rts, sfcnTsMap);

      /* Set up the mdlInfo pointer */
      {
        ssSetBlkInfo2Ptr(rts,
                         &send_receive_1_sm_model_M->NonInlinedSFcns.blkInfo2[2]);
      }

      ssSetRTWSfcnInfo(rts, send_receive_1_sm_model_M->sfcnInfo);

      /* Allocate memory of model methods 2 */
      {
        ssSetModelMethods2(rts,
                           &send_receive_1_sm_model_M->NonInlinedSFcns.methods2
                           [2]);
      }

      /* Allocate memory of model methods 3 */
      {
        ssSetModelMethods3(rts,
                           &send_receive_1_sm_model_M->NonInlinedSFcns.methods3
                           [2]);
      }

      /* Allocate memory for states auxilliary information */
      {
        ssSetStatesInfo2(rts,
                         &send_receive_1_sm_model_M->
                         NonInlinedSFcns.statesInfo2[2]);
      }

      /* inputs */
      {
        _ssSetNumInputPorts(rts, 1);
        ssSetPortInfoForInputs(rts,
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn2.inputPortInfo[0]);

        /* port 0 */
        {
          real_T const **sfcnUPtrs = (real_T const **)
            &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn2.UPtrs0;
          sfcnUPtrs[0] = &send_receive_1_sm_model_B.SFunction2;
          sfcnUPtrs[1] = &send_receive_1_sm_model_B.SFunction1_o1;
          sfcnUPtrs[2] = &send_receive_1_sm_model_B.SFunction1_o2;

          {
            int_T i1;
            const real_T *u0 = &send_receive_1_sm_model_B.SFunction1_o3[0];
            for (i1=0; i1 < 5; i1++) {
              sfcnUPtrs[i1+ 3] = &u0[i1];
            }

            sfcnUPtrs[8] = send_receive_1_sm_model_P.Constant_Value;
            sfcnUPtrs[9] = &send_receive_1_sm_model_P.Constant_Value[1];
            sfcnUPtrs[10] = &send_receive_1_sm_model_P.Constant_Value[2];
            sfcnUPtrs[11] = &send_receive_1_sm_model_P.Constant_Value[3];
            sfcnUPtrs[12] = &send_receive_1_sm_model_B.PulseGenerator1;
          }

          ssSetInputPortSignalPtrs(rts, 0, (InputPtrsType)&sfcnUPtrs[0]);
          _ssSetInputPortNumDimensions(rts, 0, 1);
          ssSetInputPortWidth(rts, 0, 13);
        }
      }

      /* path info */
      ssSetModelName(rts, "S-Function");
      ssSetPath(rts,
                "send_receive_1_sm_model/sm_model/rtlab_send_subsystem/Subsystem1/Send1/S-Function");
      ssSetRTModel(rts,send_receive_1_sm_model_M);
      ssSetParentSS(rts, (NULL));
      ssSetRootSS(rts, rts);
      ssSetVersion(rts, SIMSTRUCT_VERSION_LEVEL2);

      /* parameters */
      {
        mxArray **sfcnParams = (mxArray **)
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn2.params;
        ssSetSFcnParamsCount(rts, 1);
        ssSetSFcnParamsPtr(rts, &sfcnParams[0]);
        ssSetSFcnParam(rts, 0, (mxArray*)
                       send_receive_1_sm_model_P.SFunction_P1_Size);
      }

      /* work vectors */
      ssSetIWork(rts, (int_T *) &send_receive_1_sm_model_DWork.SFunction_IWORK[0]);

      {
        struct _ssDWorkRecord *dWorkRecord = (struct _ssDWorkRecord *)
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn2.dWork;
        struct _ssDWorkAuxRecord *dWorkAuxRecord = (struct _ssDWorkAuxRecord *)
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn2.dWorkAux;
        ssSetSFcnDWork(rts, dWorkRecord);
        ssSetSFcnDWorkAux(rts, dWorkAuxRecord);
        _ssSetNumDWork(rts, 1);

        /* IWORK */
        ssSetDWorkWidth(rts, 0, 5);
        ssSetDWorkDataType(rts, 0,SS_INTEGER);
        ssSetDWorkComplexSignal(rts, 0, 0);
        ssSetDWork(rts, 0, &send_receive_1_sm_model_DWork.SFunction_IWORK[0]);
      }

      /* registration */
      OP_SEND(rts);
      sfcnInitializeSizes(rts);
      sfcnInitializeSampleTimes(rts);

      /* adjust sample time */
      ssSetSampleTime(rts, 0, 5.0E-5);
      ssSetOffsetTime(rts, 0, 0.0);
      sfcnTsMap[0] = 0;

      /* set compiled values of dynamic vector attributes */
      ssSetInputPortWidth(rts, 0, 13);
      ssSetInputPortDataType(rts, 0, SS_DOUBLE);
      ssSetInputPortComplexSignal(rts, 0, 0);
      ssSetInputPortFrameData(rts, 0, 0);
      ssSetNumNonsampledZCs(rts, 0);

      /* Update connectivity flags for each port */
      _ssSetInputPortConnected(rts, 0, 1);

      /* Update the BufferDstPort flags for each input port */
      ssSetInputPortBufferDstPort(rts, 0, -1);
    }

    /* Level2 S-Function Block: send_receive_1_sm_model/<S2>/OpIPSocketCtrl1 (sfun_gen_async_ctrl) */
    {
      SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[3];

      /* timing info */
      time_T *sfcnPeriod =
        send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn3.sfcnPeriod;
      time_T *sfcnOffset =
        send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn3.sfcnOffset;
      int_T *sfcnTsMap =
        send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn3.sfcnTsMap;
      (void) memset((void*)sfcnPeriod, 0,
                    sizeof(time_T)*1);
      (void) memset((void*)sfcnOffset, 0,
                    sizeof(time_T)*1);
      ssSetSampleTimePtr(rts, &sfcnPeriod[0]);
      ssSetOffsetTimePtr(rts, &sfcnOffset[0]);
      ssSetSampleTimeTaskIDPtr(rts, sfcnTsMap);

      /* Set up the mdlInfo pointer */
      {
        ssSetBlkInfo2Ptr(rts,
                         &send_receive_1_sm_model_M->NonInlinedSFcns.blkInfo2[3]);
      }

      ssSetRTWSfcnInfo(rts, send_receive_1_sm_model_M->sfcnInfo);

      /* Allocate memory of model methods 2 */
      {
        ssSetModelMethods2(rts,
                           &send_receive_1_sm_model_M->NonInlinedSFcns.methods2
                           [3]);
      }

      /* Allocate memory of model methods 3 */
      {
        ssSetModelMethods3(rts,
                           &send_receive_1_sm_model_M->NonInlinedSFcns.methods3
                           [3]);
      }

      /* Allocate memory for states auxilliary information */
      {
        ssSetStatesInfo2(rts,
                         &send_receive_1_sm_model_M->
                         NonInlinedSFcns.statesInfo2[3]);
      }

      /* path info */
      ssSetModelName(rts, "OpIPSocketCtrl1");
      ssSetPath(rts, "send_receive_1_sm_model/sm_model/OpIPSocketCtrl1");
      ssSetRTModel(rts,send_receive_1_sm_model_M);
      ssSetParentSS(rts, (NULL));
      ssSetRootSS(rts, rts);
      ssSetVersion(rts, SIMSTRUCT_VERSION_LEVEL2);

      /* parameters */
      {
        mxArray **sfcnParams = (mxArray **)
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn3.params;
        ssSetSFcnParamsCount(rts, 27);
        ssSetSFcnParamsPtr(rts, &sfcnParams[0]);
        ssSetSFcnParam(rts, 0, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P1_Size);
        ssSetSFcnParam(rts, 1, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P2_Size);
        ssSetSFcnParam(rts, 2, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P3_Size);
        ssSetSFcnParam(rts, 3, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P4_Size);
        ssSetSFcnParam(rts, 4, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P5_Size);
        ssSetSFcnParam(rts, 5, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P6_Size);
        ssSetSFcnParam(rts, 6, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P7_Size);
        ssSetSFcnParam(rts, 7, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P8_Size);
        ssSetSFcnParam(rts, 8, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P9_Size);
        ssSetSFcnParam(rts, 9, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P10_Size);
        ssSetSFcnParam(rts, 10, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P11_Size);
        ssSetSFcnParam(rts, 11, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P12_Size);
        ssSetSFcnParam(rts, 12, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P13_Size);
        ssSetSFcnParam(rts, 13, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P14_Size);
        ssSetSFcnParam(rts, 14, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P15_Size);
        ssSetSFcnParam(rts, 15, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P16_Size);
        ssSetSFcnParam(rts, 16, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P17_Size);
        ssSetSFcnParam(rts, 17, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P18_Size);
        ssSetSFcnParam(rts, 18, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P19_Size);
        ssSetSFcnParam(rts, 19, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P20_Size);
        ssSetSFcnParam(rts, 20, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P21_Size);
        ssSetSFcnParam(rts, 21, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P22_Size);
        ssSetSFcnParam(rts, 22, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P23_Size);
        ssSetSFcnParam(rts, 23, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P24_Size);
        ssSetSFcnParam(rts, 24, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P25_Size);
        ssSetSFcnParam(rts, 25, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P26_Size);
        ssSetSFcnParam(rts, 26, (mxArray*)
                       send_receive_1_sm_model_P.OpIPSocketCtrl1_P27_Size);
      }

      /* work vectors */
      ssSetPWork(rts, (void **)
                 &send_receive_1_sm_model_DWork.OpIPSocketCtrl1_PWORK);

      {
        struct _ssDWorkRecord *dWorkRecord = (struct _ssDWorkRecord *)
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn3.dWork;
        struct _ssDWorkAuxRecord *dWorkAuxRecord = (struct _ssDWorkAuxRecord *)
          &send_receive_1_sm_model_M->NonInlinedSFcns.Sfcn3.dWorkAux;
        ssSetSFcnDWork(rts, dWorkRecord);
        ssSetSFcnDWorkAux(rts, dWorkAuxRecord);
        _ssSetNumDWork(rts, 1);

        /* PWORK */
        ssSetDWorkWidth(rts, 0, 1);
        ssSetDWorkDataType(rts, 0,SS_POINTER);
        ssSetDWorkComplexSignal(rts, 0, 0);
        ssSetDWork(rts, 0, &send_receive_1_sm_model_DWork.OpIPSocketCtrl1_PWORK);
      }

      /* registration */
      sfun_gen_async_ctrl(rts);
      sfcnInitializeSizes(rts);
      sfcnInitializeSampleTimes(rts);

      /* adjust sample time */
      ssSetSampleTime(rts, 0, 5.0E-5);
      ssSetOffsetTime(rts, 0, 0.0);
      sfcnTsMap[0] = 0;

      /* set compiled values of dynamic vector attributes */
      ssSetNumNonsampledZCs(rts, 0);

      /* Update connectivity flags for each port */
      /* Update the BufferDstPort flags for each input port */
    }
  }
}

/* Model terminate function */
void send_receive_1_sm_model_terminate(void)
{
  /* Level2 S-Function Block: '<S5>/S-Function2' (sfun_send_async) */
  {
    SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[0];
    sfcnTerminate(rts);
  }

  /* Level2 S-Function Block: '<S3>/S-Function1' (sfun_recv_async) */
  {
    SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[1];
    sfcnTerminate(rts);
  }

  /* Level2 S-Function Block: '<S7>/S-Function' (OP_SEND) */
  {
    SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[2];
    sfcnTerminate(rts);
  }

  /* Level2 S-Function Block: '<S2>/OpIPSocketCtrl1' (sfun_gen_async_ctrl) */
  {
    SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[3];
    sfcnTerminate(rts);
  }
}

/*========================================================================*
 * Start of GRT compatible call interface                                 *
 *========================================================================*/
void MdlOutputs(int_T tid)
{
  send_receive_1_sm_model_output(tid);
}

void MdlUpdate(int_T tid)
{
  send_receive_1_sm_model_update(tid);
}

void MdlInitializeSizes(void)
{
  send_receive_1_sm_model_M->Sizes.numContStates = (0);/* Number of continuous states */
  send_receive_1_sm_model_M->Sizes.numY = (0);/* Number of model outputs */
  send_receive_1_sm_model_M->Sizes.numU = (0);/* Number of model inputs */
  send_receive_1_sm_model_M->Sizes.sysDirFeedThru = (0);/* The model is not direct feedthrough */
  send_receive_1_sm_model_M->Sizes.numSampTimes = (1);/* Number of sample times */
  send_receive_1_sm_model_M->Sizes.numBlocks = (12);/* Number of blocks */
  send_receive_1_sm_model_M->Sizes.numBlockIO = (8);/* Number of block outputs */
  send_receive_1_sm_model_M->Sizes.numBlockPrms = (249);/* Sum of parameter "widths" */
}

void MdlInitializeSampleTimes(void)
{
}

void MdlInitialize(void)
{
  /* user code (Initialize function Body) */

  /* System '<Root>' */
  /* Opal-RT Technologies */
  opalSizeDwork = sizeof(rtDWork);

#ifdef USE_RTMODEL

  if (Opal_rtmGetNumBlockIO(pRtModel) != 0)
    opalSizeBlockIO = sizeof(rtB);
  else
    opalSizeBlockIO = 0;
  if (Opal_rtmGetNumBlockParams(pRtModel) != 0)
    opalSizeRTP = sizeof(rtP);
  else
    opalSizeRTP = 0;

#else

  if (ssGetNumBlockIO(rtS) != 0)
    opalSizeBlockIO = sizeof(rtB);
  else
    opalSizeBlockIO = 0;
  if (ssGetNumBlockParams(rtS) != 0)
    opalSizeRTP = sizeof(rtP);
  else
    opalSizeRTP = 0;

#endif

  /* InitializeConditions for Memory: '<S1>/S-Function' */
  send_receive_1_sm_model_DWork.SFunction_PreviousInput =
    send_receive_1_sm_model_P.SFunction_X0;

  /* Level2 S-Function Block: '<S5>/S-Function2' (sfun_send_async) */
  {
    SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[0];
    sfcnInitializeConditions(rts);
    if (ssGetErrorStatus(rts) != (NULL))
      return;
  }

  /* Level2 S-Function Block: '<S3>/S-Function1' (sfun_recv_async) */
  {
    SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[1];
    sfcnInitializeConditions(rts);
    if (ssGetErrorStatus(rts) != (NULL))
      return;
  }

  /* Level2 S-Function Block: '<S7>/S-Function' (OP_SEND) */
  {
    SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[2];
    sfcnInitializeConditions(rts);
    if (ssGetErrorStatus(rts) != (NULL))
      return;
  }

  /* Level2 S-Function Block: '<S2>/OpIPSocketCtrl1' (sfun_gen_async_ctrl) */
  {
    SimStruct *rts = send_receive_1_sm_model_M->childSfunctions[3];
    sfcnInitializeConditions(rts);
    if (ssGetErrorStatus(rts) != (NULL))
      return;
  }
}

void MdlStart(void)
{
  /* Start for DiscretePulseGenerator: '<S2>/data ready 2 kHz' */
  send_receive_1_sm_model_DWork.clockTickCounter = 0;

  /* Start for DiscretePulseGenerator: '<S2>/Pulse Generator1' */
  send_receive_1_sm_model_DWork.clockTickCounter_h = 0;
  MdlInitialize();
}

void MdlTerminate(void)
{
  send_receive_1_sm_model_terminate();
}

RT_MODEL_send_receive_1_sm_model *send_receive_1_sm_model(void)
{
  send_receive_1_sm_model_initialize(1);
  return send_receive_1_sm_model_M;
}

/*========================================================================*
 * End of GRT compatible call interface                                   *
 *========================================================================*/
