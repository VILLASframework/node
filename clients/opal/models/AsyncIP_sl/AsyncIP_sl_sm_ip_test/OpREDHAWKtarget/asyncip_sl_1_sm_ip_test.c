/*
 * asyncip_sl_1_sm_ip_test.c
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
#include "asyncip_sl_1_sm_ip_test.h"
#include "asyncip_sl_1_sm_ip_test_private.h"

/* Block signals (auto storage) */
BlockIO_asyncip_sl_1_sm_ip_test asyncip_sl_1_sm_ip_test_B;

/* Block states (auto storage) */
D_Work_asyncip_sl_1_sm_ip_test asyncip_sl_1_sm_ip_test_DWork;

/* Real-time model */
RT_MODEL_asyncip_sl_1_sm_ip_test asyncip_sl_1_sm_ip_test_M_;
RT_MODEL_asyncip_sl_1_sm_ip_test *const asyncip_sl_1_sm_ip_test_M =
  &asyncip_sl_1_sm_ip_test_M_;
static void rate_scheduler(void);

/*
 *   This function updates active task flag for each subrate.
 * The function is called at model base rate, hence the
 * generated code self-manages all its subrates.
 */
static void rate_scheduler(void)
{
  /* Compute which subrates run during the next base time step.  Subrates
   * are an integer multiple of the base rate counter.  Therefore, the subtask
   * counter is reset when it reaches its limit (zero means run).
   */
  (asyncip_sl_1_sm_ip_test_M->Timing.TaskCounters.TID[1])++;
  if ((asyncip_sl_1_sm_ip_test_M->Timing.TaskCounters.TID[1]) > 19) {/* Sample time: [0.001s, 0.0s] */
    asyncip_sl_1_sm_ip_test_M->Timing.TaskCounters.TID[1] = 0;
  }

  asyncip_sl_1_sm_ip_test_M->Timing.sampleHits[1] =
    (asyncip_sl_1_sm_ip_test_M->Timing.TaskCounters.TID[1] == 0);
}

/* Model output function */
static void asyncip_sl_1_sm_ip_test_output(int_T tid)
{
  /* Memory: '<S1>/S-Function' */
  asyncip_sl_1_sm_ip_test_B.SFunction =
    asyncip_sl_1_sm_ip_test_DWork.SFunction_PreviousInput;

  /* Sum: '<S1>/Sum' incorporates:
   *  Constant: '<S1>/S-Function1'
   */
  asyncip_sl_1_sm_ip_test_B.Sum = asyncip_sl_1_sm_ip_test_P.SFunction1_Value +
    asyncip_sl_1_sm_ip_test_B.SFunction;

  /* Stop: '<S1>/Stop Simulation' */
  if (asyncip_sl_1_sm_ip_test_B.Sum != 0.0) {
    rtmSetStopRequested(asyncip_sl_1_sm_ip_test_M, 1);
  }

  /* End of Stop: '<S1>/Stop Simulation' */
  if (asyncip_sl_1_sm_ip_test_M->Timing.TaskCounters.TID[1] == 0) {
    /* DiscretePulseGenerator: '<S2>/data ready 1 kHz' */
    asyncip_sl_1_sm_ip_test_B.dataready1kHz = ((real_T)
      asyncip_sl_1_sm_ip_test_DWork.clockTickCounter <
      asyncip_sl_1_sm_ip_test_P.dataready1kHz_Duty) &&
      (asyncip_sl_1_sm_ip_test_DWork.clockTickCounter >= 0) ?
      asyncip_sl_1_sm_ip_test_P.dataready1kHz_Amp : 0.0;
    if ((real_T)asyncip_sl_1_sm_ip_test_DWork.clockTickCounter >=
        asyncip_sl_1_sm_ip_test_P.dataready1kHz_Period - 1.0) {
      asyncip_sl_1_sm_ip_test_DWork.clockTickCounter = 0;
    } else {
      asyncip_sl_1_sm_ip_test_DWork.clockTickCounter =
        asyncip_sl_1_sm_ip_test_DWork.clockTickCounter + 1;
    }

    /* End of DiscretePulseGenerator: '<S2>/data ready 1 kHz' */
  }

  /* DiscretePulseGenerator: '<S2>/Pulse Generator' */
  asyncip_sl_1_sm_ip_test_B.PulseGenerator = ((real_T)
    asyncip_sl_1_sm_ip_test_DWork.clockTickCounter_c <
    asyncip_sl_1_sm_ip_test_P.PulseGenerator_Duty) &&
    (asyncip_sl_1_sm_ip_test_DWork.clockTickCounter_c >= 0) ?
    asyncip_sl_1_sm_ip_test_P.PulseGenerator_Amp : 0.0;
  if ((real_T)asyncip_sl_1_sm_ip_test_DWork.clockTickCounter_c >=
      asyncip_sl_1_sm_ip_test_P.PulseGenerator_Period - 1.0) {
    asyncip_sl_1_sm_ip_test_DWork.clockTickCounter_c = 0;
  } else {
    asyncip_sl_1_sm_ip_test_DWork.clockTickCounter_c =
      asyncip_sl_1_sm_ip_test_DWork.clockTickCounter_c + 1;
  }

  /* End of DiscretePulseGenerator: '<S2>/Pulse Generator' */

  /* Level2 S-Function Block: '<S5>/S-Function2' (sfun_send_async) */
  {
    SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[0];
    sfcnOutputs(rts, 0);
  }

  /* Level2 S-Function Block: '<S3>/S-Function1' (sfun_recv_async) */
  {
    SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[1];
    sfcnOutputs(rts, 0);
  }

  /* Level2 S-Function Block: '<S7>/S-Function' (OP_SEND) */
  {
    SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[2];
    sfcnOutputs(rts, 0);
  }

  /* Level2 S-Function Block: '<S2>/OpIPSocketCtrl1' (sfun_gen_async_ctrl) */
  {
    SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[3];
    sfcnOutputs(rts, 0);
  }

  /* tid is required for a uniform function interface.
   * Argument tid is not used in the function. */
  UNUSED_PARAMETER(tid);
}

/* Model update function */
static void asyncip_sl_1_sm_ip_test_update(int_T tid)
{
  /* Update for Memory: '<S1>/S-Function' */
  asyncip_sl_1_sm_ip_test_DWork.SFunction_PreviousInput =
    asyncip_sl_1_sm_ip_test_B.Sum;

  /* Update absolute time for base rate */
  /* The "clockTick0" counts the number of times the code of this task has
   * been executed. The absolute time is the multiplication of "clockTick0"
   * and "Timing.stepSize0". Size of "clockTick0" ensures timer will not
   * overflow during the application lifespan selected.
   * Timer of this task consists of two 32 bit unsigned integers.
   * The two integers represent the low bits Timing.clockTick0 and the high bits
   * Timing.clockTickH0. When the low bit overflows to 0, the high bits increment.
   */
  if (!(++asyncip_sl_1_sm_ip_test_M->Timing.clockTick0)) {
    ++asyncip_sl_1_sm_ip_test_M->Timing.clockTickH0;
  }

  asyncip_sl_1_sm_ip_test_M->Timing.t[0] =
    asyncip_sl_1_sm_ip_test_M->Timing.clockTick0 *
    asyncip_sl_1_sm_ip_test_M->Timing.stepSize0 +
    asyncip_sl_1_sm_ip_test_M->Timing.clockTickH0 *
    asyncip_sl_1_sm_ip_test_M->Timing.stepSize0 * 4294967296.0;
  if (asyncip_sl_1_sm_ip_test_M->Timing.TaskCounters.TID[1] == 0) {
    /* Update absolute timer for sample time: [0.001s, 0.0s] */
    /* The "clockTick1" counts the number of times the code of this task has
     * been executed. The absolute time is the multiplication of "clockTick1"
     * and "Timing.stepSize1". Size of "clockTick1" ensures timer will not
     * overflow during the application lifespan selected.
     * Timer of this task consists of two 32 bit unsigned integers.
     * The two integers represent the low bits Timing.clockTick1 and the high bits
     * Timing.clockTickH1. When the low bit overflows to 0, the high bits increment.
     */
    if (!(++asyncip_sl_1_sm_ip_test_M->Timing.clockTick1)) {
      ++asyncip_sl_1_sm_ip_test_M->Timing.clockTickH1;
    }

    asyncip_sl_1_sm_ip_test_M->Timing.t[1] =
      asyncip_sl_1_sm_ip_test_M->Timing.clockTick1 *
      asyncip_sl_1_sm_ip_test_M->Timing.stepSize1 +
      asyncip_sl_1_sm_ip_test_M->Timing.clockTickH1 *
      asyncip_sl_1_sm_ip_test_M->Timing.stepSize1 * 4294967296.0;
  }

  rate_scheduler();

  /* tid is required for a uniform function interface.
   * Argument tid is not used in the function. */
  UNUSED_PARAMETER(tid);
}

/* Model initialize function */
void asyncip_sl_1_sm_ip_test_initialize(boolean_T firstTime)
{
  (void)firstTime;

  /* Registration code */

  /* initialize non-finites */
  rt_InitInfAndNaN(sizeof(real_T));

  /* initialize real-time model */
  (void) memset((void *)asyncip_sl_1_sm_ip_test_M, 0,
                sizeof(RT_MODEL_asyncip_sl_1_sm_ip_test));
  rtsiSetSolverName(&asyncip_sl_1_sm_ip_test_M->solverInfo,"FixedStepDiscrete");
  asyncip_sl_1_sm_ip_test_M->solverInfoPtr =
    (&asyncip_sl_1_sm_ip_test_M->solverInfo);

  /* Initialize timing info */
  {
    int_T *mdlTsMap = asyncip_sl_1_sm_ip_test_M->Timing.sampleTimeTaskIDArray;
    mdlTsMap[0] = 0;
    mdlTsMap[1] = 1;
    asyncip_sl_1_sm_ip_test_M->Timing.sampleTimeTaskIDPtr = (&mdlTsMap[0]);
    asyncip_sl_1_sm_ip_test_M->Timing.sampleTimes =
      (&asyncip_sl_1_sm_ip_test_M->Timing.sampleTimesArray[0]);
    asyncip_sl_1_sm_ip_test_M->Timing.offsetTimes =
      (&asyncip_sl_1_sm_ip_test_M->Timing.offsetTimesArray[0]);

    /* task periods */
    asyncip_sl_1_sm_ip_test_M->Timing.sampleTimes[0] = (5.0E-5);
    asyncip_sl_1_sm_ip_test_M->Timing.sampleTimes[1] = (0.001);

    /* task offsets */
    asyncip_sl_1_sm_ip_test_M->Timing.offsetTimes[0] = (0.0);
    asyncip_sl_1_sm_ip_test_M->Timing.offsetTimes[1] = (0.0);
  }

  rtmSetTPtr(asyncip_sl_1_sm_ip_test_M,
             &asyncip_sl_1_sm_ip_test_M->Timing.tArray[0]);

  {
    int_T *mdlSampleHits = asyncip_sl_1_sm_ip_test_M->Timing.sampleHitArray;
    mdlSampleHits[0] = 1;
    mdlSampleHits[1] = 1;
    asyncip_sl_1_sm_ip_test_M->Timing.sampleHits = (&mdlSampleHits[0]);
  }

  rtmSetTFinal(asyncip_sl_1_sm_ip_test_M, -1);
  asyncip_sl_1_sm_ip_test_M->Timing.stepSize0 = 5.0E-5;
  asyncip_sl_1_sm_ip_test_M->Timing.stepSize1 = 0.001;

  /* Setup for data logging */
  {
    static RTWLogInfo rt_DataLoggingInfo;
    asyncip_sl_1_sm_ip_test_M->rtwLogInfo = &rt_DataLoggingInfo;
  }

  /* Setup for data logging */
  {
    rtliSetLogXSignalInfo(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, (NULL));
    rtliSetLogXSignalPtrs(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, (NULL));
    rtliSetLogT(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, "");
    rtliSetLogX(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, "");
    rtliSetLogXFinal(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, "");
    rtliSetSigLog(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, "");
    rtliSetLogVarNameModifier(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, "rt_");
    rtliSetLogFormat(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, 0);
    rtliSetLogMaxRows(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, 0);
    rtliSetLogDecimation(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, 1);
    rtliSetLogY(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, "");
    rtliSetLogYSignalInfo(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, (NULL));
    rtliSetLogYSignalPtrs(asyncip_sl_1_sm_ip_test_M->rtwLogInfo, (NULL));
  }

  asyncip_sl_1_sm_ip_test_M->solverInfoPtr =
    (&asyncip_sl_1_sm_ip_test_M->solverInfo);
  asyncip_sl_1_sm_ip_test_M->Timing.stepSize = (5.0E-5);
  rtsiSetFixedStepSize(&asyncip_sl_1_sm_ip_test_M->solverInfo, 5.0E-5);
  rtsiSetSolverMode(&asyncip_sl_1_sm_ip_test_M->solverInfo,
                    SOLVER_MODE_SINGLETASKING);

  /* block I/O */
  asyncip_sl_1_sm_ip_test_M->ModelData.blockIO = ((void *)
    &asyncip_sl_1_sm_ip_test_B);

  {
    int_T i;
    for (i = 0; i < 5; i++) {
      asyncip_sl_1_sm_ip_test_B.SFunction1_o3[i] = 0.0;
    }

    asyncip_sl_1_sm_ip_test_B.SFunction = 0.0;
    asyncip_sl_1_sm_ip_test_B.Sum = 0.0;
    asyncip_sl_1_sm_ip_test_B.dataready1kHz = 0.0;
    asyncip_sl_1_sm_ip_test_B.PulseGenerator = 0.0;
    asyncip_sl_1_sm_ip_test_B.SFunction2 = 0.0;
    asyncip_sl_1_sm_ip_test_B.SFunction1_o1 = 0.0;
    asyncip_sl_1_sm_ip_test_B.SFunction1_o2 = 0.0;
  }

  /* parameters */
  asyncip_sl_1_sm_ip_test_M->ModelData.defaultParam = ((real_T *)
    &asyncip_sl_1_sm_ip_test_P);

  /* states (dwork) */
  asyncip_sl_1_sm_ip_test_M->Work.dwork = ((void *)
    &asyncip_sl_1_sm_ip_test_DWork);
  (void) memset((void *)&asyncip_sl_1_sm_ip_test_DWork, 0,
                sizeof(D_Work_asyncip_sl_1_sm_ip_test));
  asyncip_sl_1_sm_ip_test_DWork.SFunction_PreviousInput = 0.0;

  /* child S-Function registration */
  {
    RTWSfcnInfo *sfcnInfo = &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.sfcnInfo;
    asyncip_sl_1_sm_ip_test_M->sfcnInfo = (sfcnInfo);
    rtssSetErrorStatusPtr(sfcnInfo, (&rtmGetErrorStatus
      (asyncip_sl_1_sm_ip_test_M)));
    rtssSetNumRootSampTimesPtr(sfcnInfo,
      &asyncip_sl_1_sm_ip_test_M->Sizes.numSampTimes);
    asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.taskTimePtrs[0] = &(rtmGetTPtr
      (asyncip_sl_1_sm_ip_test_M)[0]);
    asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.taskTimePtrs[1] = &(rtmGetTPtr
      (asyncip_sl_1_sm_ip_test_M)[1]);
    rtssSetTPtrPtr(sfcnInfo,
                   asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.taskTimePtrs);
    rtssSetTStartPtr(sfcnInfo, &rtmGetTStart(asyncip_sl_1_sm_ip_test_M));
    rtssSetTFinalPtr(sfcnInfo, &rtmGetTFinal(asyncip_sl_1_sm_ip_test_M));
    rtssSetTimeOfLastOutputPtr(sfcnInfo, &rtmGetTimeOfLastOutput
      (asyncip_sl_1_sm_ip_test_M));
    rtssSetStepSizePtr(sfcnInfo, &asyncip_sl_1_sm_ip_test_M->Timing.stepSize);
    rtssSetStopRequestedPtr(sfcnInfo, &rtmGetStopRequested
      (asyncip_sl_1_sm_ip_test_M));
    rtssSetDerivCacheNeedsResetPtr(sfcnInfo,
      &asyncip_sl_1_sm_ip_test_M->ModelData.derivCacheNeedsReset);
    rtssSetZCCacheNeedsResetPtr(sfcnInfo,
      &asyncip_sl_1_sm_ip_test_M->ModelData.zCCacheNeedsReset);
    rtssSetBlkStateChangePtr(sfcnInfo,
      &asyncip_sl_1_sm_ip_test_M->ModelData.blkStateChange);
    rtssSetSampleHitsPtr(sfcnInfo, &asyncip_sl_1_sm_ip_test_M->Timing.sampleHits);
    rtssSetPerTaskSampleHitsPtr(sfcnInfo,
      &asyncip_sl_1_sm_ip_test_M->Timing.perTaskSampleHits);
    rtssSetSimModePtr(sfcnInfo, &asyncip_sl_1_sm_ip_test_M->simMode);
    rtssSetSolverInfoPtr(sfcnInfo, &asyncip_sl_1_sm_ip_test_M->solverInfoPtr);
  }

  asyncip_sl_1_sm_ip_test_M->Sizes.numSFcns = (4);

  /* register each child */
  {
    (void) memset((void *)
                  &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.childSFunctions[0],
                  0,
                  4*sizeof(SimStruct));
    asyncip_sl_1_sm_ip_test_M->childSfunctions =
      (&asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.childSFunctionPtrs[0]);
    asyncip_sl_1_sm_ip_test_M->childSfunctions[0] =
      (&asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.childSFunctions[0]);
    asyncip_sl_1_sm_ip_test_M->childSfunctions[1] =
      (&asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.childSFunctions[1]);
    asyncip_sl_1_sm_ip_test_M->childSfunctions[2] =
      (&asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.childSFunctions[2]);
    asyncip_sl_1_sm_ip_test_M->childSfunctions[3] =
      (&asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.childSFunctions[3]);

    /* Level2 S-Function Block: asyncip_sl_1_sm_ip_test/<S5>/S-Function2 (sfun_send_async) */
    {
      SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[0];

      /* timing info */
      time_T *sfcnPeriod =
        asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn0.sfcnPeriod;
      time_T *sfcnOffset =
        asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn0.sfcnOffset;
      int_T *sfcnTsMap =
        asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn0.sfcnTsMap;
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
                         &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.blkInfo2[0]);
      }

      ssSetRTWSfcnInfo(rts, asyncip_sl_1_sm_ip_test_M->sfcnInfo);

      /* Allocate memory of model methods 2 */
      {
        ssSetModelMethods2(rts,
                           &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.methods2
                           [0]);
      }

      /* Allocate memory of model methods 3 */
      {
        ssSetModelMethods3(rts,
                           &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.methods3
                           [0]);
      }

      /* Allocate memory for states auxilliary information */
      {
        ssSetStatesInfo2(rts,
                         &asyncip_sl_1_sm_ip_test_M->
                         NonInlinedSFcns.statesInfo2[0]);
      }

      /* inputs */
      {
        _ssSetNumInputPorts(rts, 2);
        ssSetPortInfoForInputs(rts,
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn0.inputPortInfo[0]);

        /* port 0 */
        {
          real_T const **sfcnUPtrs = (real_T const **)
            &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn0.UPtrs0;
          sfcnUPtrs[0] = &asyncip_sl_1_sm_ip_test_B.dataready1kHz;
          ssSetInputPortSignalPtrs(rts, 0, (InputPtrsType)&sfcnUPtrs[0]);
          _ssSetInputPortNumDimensions(rts, 0, 1);
          ssSetInputPortWidth(rts, 0, 1);
        }

        /* port 1 */
        {
          real_T const **sfcnUPtrs = (real_T const **)
            &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn0.UPtrs1;
          sfcnUPtrs[0] = asyncip_sl_1_sm_ip_test_P.constants_Value;
          sfcnUPtrs[1] = &asyncip_sl_1_sm_ip_test_P.constants_Value[1];
          sfcnUPtrs[2] = &asyncip_sl_1_sm_ip_test_P.constants_Value[2];
          sfcnUPtrs[3] = &asyncip_sl_1_sm_ip_test_P.constants_Value[3];
          sfcnUPtrs[4] = &asyncip_sl_1_sm_ip_test_B.PulseGenerator;
          ssSetInputPortSignalPtrs(rts, 1, (InputPtrsType)&sfcnUPtrs[0]);
          _ssSetInputPortNumDimensions(rts, 1, 1);
          ssSetInputPortWidth(rts, 1, 5);
        }
      }

      /* outputs */
      {
        ssSetPortInfoForOutputs(rts,
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn0.outputPortInfo[0]);
        _ssSetNumOutputPorts(rts, 1);

        /* port 0 */
        {
          _ssSetOutputPortNumDimensions(rts, 0, 1);
          ssSetOutputPortWidth(rts, 0, 1);
          ssSetOutputPortSignal(rts, 0, ((real_T *)
            &asyncip_sl_1_sm_ip_test_B.SFunction2));
        }
      }

      /* path info */
      ssSetModelName(rts, "S-Function2");
      ssSetPath(rts,
                "asyncip_sl_1_sm_ip_test/sm_ip_test/send message 1/S-Function2");
      ssSetRTModel(rts,asyncip_sl_1_sm_ip_test_M);
      ssSetParentSS(rts, (NULL));
      ssSetRootSS(rts, rts);
      ssSetVersion(rts, SIMSTRUCT_VERSION_LEVEL2);

      /* parameters */
      {
        mxArray **sfcnParams = (mxArray **)
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn0.params;
        ssSetSFcnParamsCount(rts, 13);
        ssSetSFcnParamsPtr(rts, &sfcnParams[0]);
        ssSetSFcnParam(rts, 0, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P1_Size);
        ssSetSFcnParam(rts, 1, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P2_Size);
        ssSetSFcnParam(rts, 2, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P3_Size);
        ssSetSFcnParam(rts, 3, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P4_Size);
        ssSetSFcnParam(rts, 4, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P5_Size);
        ssSetSFcnParam(rts, 5, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P6_Size);
        ssSetSFcnParam(rts, 6, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P7_Size);
        ssSetSFcnParam(rts, 7, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P8_Size);
        ssSetSFcnParam(rts, 8, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P9_Size);
        ssSetSFcnParam(rts, 9, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P10_Size);
        ssSetSFcnParam(rts, 10, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P11_Size);
        ssSetSFcnParam(rts, 11, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P12_Size);
        ssSetSFcnParam(rts, 12, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction2_P13_Size);
      }

      /* work vectors */
      ssSetPWork(rts, (void **) &asyncip_sl_1_sm_ip_test_DWork.SFunction2_PWORK);

      {
        struct _ssDWorkRecord *dWorkRecord = (struct _ssDWorkRecord *)
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn0.dWork;
        struct _ssDWorkAuxRecord *dWorkAuxRecord = (struct _ssDWorkAuxRecord *)
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn0.dWorkAux;
        ssSetSFcnDWork(rts, dWorkRecord);
        ssSetSFcnDWorkAux(rts, dWorkAuxRecord);
        _ssSetNumDWork(rts, 1);

        /* PWORK */
        ssSetDWorkWidth(rts, 0, 1);
        ssSetDWorkDataType(rts, 0,SS_POINTER);
        ssSetDWorkComplexSignal(rts, 0, 0);
        ssSetDWork(rts, 0, &asyncip_sl_1_sm_ip_test_DWork.SFunction2_PWORK);
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

    /* Level2 S-Function Block: asyncip_sl_1_sm_ip_test/<S3>/S-Function1 (sfun_recv_async) */
    {
      SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[1];

      /* timing info */
      time_T *sfcnPeriod =
        asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn1.sfcnPeriod;
      time_T *sfcnOffset =
        asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn1.sfcnOffset;
      int_T *sfcnTsMap =
        asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn1.sfcnTsMap;
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
                         &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.blkInfo2[1]);
      }

      ssSetRTWSfcnInfo(rts, asyncip_sl_1_sm_ip_test_M->sfcnInfo);

      /* Allocate memory of model methods 2 */
      {
        ssSetModelMethods2(rts,
                           &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.methods2
                           [1]);
      }

      /* Allocate memory of model methods 3 */
      {
        ssSetModelMethods3(rts,
                           &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.methods3
                           [1]);
      }

      /* Allocate memory for states auxilliary information */
      {
        ssSetStatesInfo2(rts,
                         &asyncip_sl_1_sm_ip_test_M->
                         NonInlinedSFcns.statesInfo2[1]);
      }

      /* inputs */
      {
        _ssSetNumInputPorts(rts, 1);
        ssSetPortInfoForInputs(rts,
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn1.inputPortInfo[0]);

        /* port 0 */
        {
          real_T const **sfcnUPtrs = (real_T const **)
            &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn1.UPtrs0;
          sfcnUPtrs[0] = &asyncip_sl_1_sm_ip_test_P.timeout_Value;
          ssSetInputPortSignalPtrs(rts, 0, (InputPtrsType)&sfcnUPtrs[0]);
          _ssSetInputPortNumDimensions(rts, 0, 1);
          ssSetInputPortWidth(rts, 0, 1);
        }
      }

      /* outputs */
      {
        ssSetPortInfoForOutputs(rts,
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn1.outputPortInfo[0]);
        _ssSetNumOutputPorts(rts, 3);

        /* port 0 */
        {
          _ssSetOutputPortNumDimensions(rts, 0, 1);
          ssSetOutputPortWidth(rts, 0, 1);
          ssSetOutputPortSignal(rts, 0, ((real_T *)
            &asyncip_sl_1_sm_ip_test_B.SFunction1_o1));
        }

        /* port 1 */
        {
          _ssSetOutputPortNumDimensions(rts, 1, 1);
          ssSetOutputPortWidth(rts, 1, 1);
          ssSetOutputPortSignal(rts, 1, ((real_T *)
            &asyncip_sl_1_sm_ip_test_B.SFunction1_o2));
        }

        /* port 2 */
        {
          _ssSetOutputPortNumDimensions(rts, 2, 1);
          ssSetOutputPortWidth(rts, 2, 5);
          ssSetOutputPortSignal(rts, 2, ((real_T *)
            asyncip_sl_1_sm_ip_test_B.SFunction1_o3));
        }
      }

      /* path info */
      ssSetModelName(rts, "S-Function1");
      ssSetPath(rts,
                "asyncip_sl_1_sm_ip_test/sm_ip_test/receive message 1/S-Function1");
      ssSetRTModel(rts,asyncip_sl_1_sm_ip_test_M);
      ssSetParentSS(rts, (NULL));
      ssSetRootSS(rts, rts);
      ssSetVersion(rts, SIMSTRUCT_VERSION_LEVEL2);

      /* parameters */
      {
        mxArray **sfcnParams = (mxArray **)
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn1.params;
        ssSetSFcnParamsCount(rts, 12);
        ssSetSFcnParamsPtr(rts, &sfcnParams[0]);
        ssSetSFcnParam(rts, 0, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction1_P1_Size);
        ssSetSFcnParam(rts, 1, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction1_P2_Size);
        ssSetSFcnParam(rts, 2, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction1_P3_Size);
        ssSetSFcnParam(rts, 3, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction1_P4_Size);
        ssSetSFcnParam(rts, 4, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction1_P5_Size);
        ssSetSFcnParam(rts, 5, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction1_P6_Size);
        ssSetSFcnParam(rts, 6, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction1_P7_Size);
        ssSetSFcnParam(rts, 7, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction1_P8_Size);
        ssSetSFcnParam(rts, 8, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction1_P9_Size);
        ssSetSFcnParam(rts, 9, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction1_P10_Size);
        ssSetSFcnParam(rts, 10, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction1_P11_Size);
        ssSetSFcnParam(rts, 11, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction1_P12_Size);
      }

      /* work vectors */
      ssSetPWork(rts, (void **) &asyncip_sl_1_sm_ip_test_DWork.SFunction1_PWORK);

      {
        struct _ssDWorkRecord *dWorkRecord = (struct _ssDWorkRecord *)
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn1.dWork;
        struct _ssDWorkAuxRecord *dWorkAuxRecord = (struct _ssDWorkAuxRecord *)
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn1.dWorkAux;
        ssSetSFcnDWork(rts, dWorkRecord);
        ssSetSFcnDWorkAux(rts, dWorkAuxRecord);
        _ssSetNumDWork(rts, 1);

        /* PWORK */
        ssSetDWorkWidth(rts, 0, 1);
        ssSetDWorkDataType(rts, 0,SS_POINTER);
        ssSetDWorkComplexSignal(rts, 0, 0);
        ssSetDWork(rts, 0, &asyncip_sl_1_sm_ip_test_DWork.SFunction1_PWORK);
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

    /* Level2 S-Function Block: asyncip_sl_1_sm_ip_test/<S7>/S-Function (OP_SEND) */
    {
      SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[2];

      /* timing info */
      time_T *sfcnPeriod =
        asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn2.sfcnPeriod;
      time_T *sfcnOffset =
        asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn2.sfcnOffset;
      int_T *sfcnTsMap =
        asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn2.sfcnTsMap;
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
                         &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.blkInfo2[2]);
      }

      ssSetRTWSfcnInfo(rts, asyncip_sl_1_sm_ip_test_M->sfcnInfo);

      /* Allocate memory of model methods 2 */
      {
        ssSetModelMethods2(rts,
                           &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.methods2
                           [2]);
      }

      /* Allocate memory of model methods 3 */
      {
        ssSetModelMethods3(rts,
                           &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.methods3
                           [2]);
      }

      /* Allocate memory for states auxilliary information */
      {
        ssSetStatesInfo2(rts,
                         &asyncip_sl_1_sm_ip_test_M->
                         NonInlinedSFcns.statesInfo2[2]);
      }

      /* inputs */
      {
        _ssSetNumInputPorts(rts, 1);
        ssSetPortInfoForInputs(rts,
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn2.inputPortInfo[0]);

        /* port 0 */
        {
          real_T const **sfcnUPtrs = (real_T const **)
            &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn2.UPtrs0;
          sfcnUPtrs[0] = &asyncip_sl_1_sm_ip_test_B.SFunction2;
          sfcnUPtrs[1] = &asyncip_sl_1_sm_ip_test_B.SFunction1_o1;
          sfcnUPtrs[2] = &asyncip_sl_1_sm_ip_test_B.SFunction1_o2;

          {
            int_T i1;
            const real_T *u0 = &asyncip_sl_1_sm_ip_test_B.SFunction1_o3[0];
            for (i1=0; i1 < 5; i1++) {
              sfcnUPtrs[i1+ 3] = &u0[i1];
            }

            sfcnUPtrs[8] = asyncip_sl_1_sm_ip_test_P.constants_Value;
            sfcnUPtrs[9] = &asyncip_sl_1_sm_ip_test_P.constants_Value[1];
            sfcnUPtrs[10] = &asyncip_sl_1_sm_ip_test_P.constants_Value[2];
            sfcnUPtrs[11] = &asyncip_sl_1_sm_ip_test_P.constants_Value[3];
            sfcnUPtrs[12] = &asyncip_sl_1_sm_ip_test_B.PulseGenerator;
          }

          ssSetInputPortSignalPtrs(rts, 0, (InputPtrsType)&sfcnUPtrs[0]);
          _ssSetInputPortNumDimensions(rts, 0, 1);
          ssSetInputPortWidth(rts, 0, 13);
        }
      }

      /* path info */
      ssSetModelName(rts, "S-Function");
      ssSetPath(rts,
                "asyncip_sl_1_sm_ip_test/sm_ip_test/rtlab_send_subsystem/Subsystem1/Send1/S-Function");
      ssSetRTModel(rts,asyncip_sl_1_sm_ip_test_M);
      ssSetParentSS(rts, (NULL));
      ssSetRootSS(rts, rts);
      ssSetVersion(rts, SIMSTRUCT_VERSION_LEVEL2);

      /* parameters */
      {
        mxArray **sfcnParams = (mxArray **)
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn2.params;
        ssSetSFcnParamsCount(rts, 1);
        ssSetSFcnParamsPtr(rts, &sfcnParams[0]);
        ssSetSFcnParam(rts, 0, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.SFunction_P1_Size);
      }

      /* work vectors */
      ssSetIWork(rts, (int_T *) &asyncip_sl_1_sm_ip_test_DWork.SFunction_IWORK[0]);

      {
        struct _ssDWorkRecord *dWorkRecord = (struct _ssDWorkRecord *)
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn2.dWork;
        struct _ssDWorkAuxRecord *dWorkAuxRecord = (struct _ssDWorkAuxRecord *)
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn2.dWorkAux;
        ssSetSFcnDWork(rts, dWorkRecord);
        ssSetSFcnDWorkAux(rts, dWorkAuxRecord);
        _ssSetNumDWork(rts, 1);

        /* IWORK */
        ssSetDWorkWidth(rts, 0, 5);
        ssSetDWorkDataType(rts, 0,SS_INTEGER);
        ssSetDWorkComplexSignal(rts, 0, 0);
        ssSetDWork(rts, 0, &asyncip_sl_1_sm_ip_test_DWork.SFunction_IWORK[0]);
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

    /* Level2 S-Function Block: asyncip_sl_1_sm_ip_test/<S2>/OpIPSocketCtrl1 (sfun_gen_async_ctrl) */
    {
      SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[3];

      /* timing info */
      time_T *sfcnPeriod =
        asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn3.sfcnPeriod;
      time_T *sfcnOffset =
        asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn3.sfcnOffset;
      int_T *sfcnTsMap =
        asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn3.sfcnTsMap;
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
                         &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.blkInfo2[3]);
      }

      ssSetRTWSfcnInfo(rts, asyncip_sl_1_sm_ip_test_M->sfcnInfo);

      /* Allocate memory of model methods 2 */
      {
        ssSetModelMethods2(rts,
                           &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.methods2
                           [3]);
      }

      /* Allocate memory of model methods 3 */
      {
        ssSetModelMethods3(rts,
                           &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.methods3
                           [3]);
      }

      /* Allocate memory for states auxilliary information */
      {
        ssSetStatesInfo2(rts,
                         &asyncip_sl_1_sm_ip_test_M->
                         NonInlinedSFcns.statesInfo2[3]);
      }

      /* path info */
      ssSetModelName(rts, "OpIPSocketCtrl1");
      ssSetPath(rts, "asyncip_sl_1_sm_ip_test/sm_ip_test/OpIPSocketCtrl1");
      ssSetRTModel(rts,asyncip_sl_1_sm_ip_test_M);
      ssSetParentSS(rts, (NULL));
      ssSetRootSS(rts, rts);
      ssSetVersion(rts, SIMSTRUCT_VERSION_LEVEL2);

      /* parameters */
      {
        mxArray **sfcnParams = (mxArray **)
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn3.params;
        ssSetSFcnParamsCount(rts, 27);
        ssSetSFcnParamsPtr(rts, &sfcnParams[0]);
        ssSetSFcnParam(rts, 0, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P1_Size);
        ssSetSFcnParam(rts, 1, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P2_Size);
        ssSetSFcnParam(rts, 2, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P3_Size);
        ssSetSFcnParam(rts, 3, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P4_Size);
        ssSetSFcnParam(rts, 4, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P5_Size);
        ssSetSFcnParam(rts, 5, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P6_Size);
        ssSetSFcnParam(rts, 6, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P7_Size);
        ssSetSFcnParam(rts, 7, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P8_Size);
        ssSetSFcnParam(rts, 8, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P9_Size);
        ssSetSFcnParam(rts, 9, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P10_Size);
        ssSetSFcnParam(rts, 10, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P11_Size);
        ssSetSFcnParam(rts, 11, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P12_Size);
        ssSetSFcnParam(rts, 12, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P13_Size);
        ssSetSFcnParam(rts, 13, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P14_Size);
        ssSetSFcnParam(rts, 14, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P15_Size);
        ssSetSFcnParam(rts, 15, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P16_Size);
        ssSetSFcnParam(rts, 16, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P17_Size);
        ssSetSFcnParam(rts, 17, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P18_Size);
        ssSetSFcnParam(rts, 18, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P19_Size);
        ssSetSFcnParam(rts, 19, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P20_Size);
        ssSetSFcnParam(rts, 20, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P21_Size);
        ssSetSFcnParam(rts, 21, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P22_Size);
        ssSetSFcnParam(rts, 22, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P23_Size);
        ssSetSFcnParam(rts, 23, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P24_Size);
        ssSetSFcnParam(rts, 24, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P25_Size);
        ssSetSFcnParam(rts, 25, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P26_Size);
        ssSetSFcnParam(rts, 26, (mxArray*)
                       asyncip_sl_1_sm_ip_test_P.OpIPSocketCtrl1_P27_Size);
      }

      /* work vectors */
      ssSetPWork(rts, (void **)
                 &asyncip_sl_1_sm_ip_test_DWork.OpIPSocketCtrl1_PWORK);

      {
        struct _ssDWorkRecord *dWorkRecord = (struct _ssDWorkRecord *)
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn3.dWork;
        struct _ssDWorkAuxRecord *dWorkAuxRecord = (struct _ssDWorkAuxRecord *)
          &asyncip_sl_1_sm_ip_test_M->NonInlinedSFcns.Sfcn3.dWorkAux;
        ssSetSFcnDWork(rts, dWorkRecord);
        ssSetSFcnDWorkAux(rts, dWorkAuxRecord);
        _ssSetNumDWork(rts, 1);

        /* PWORK */
        ssSetDWorkWidth(rts, 0, 1);
        ssSetDWorkDataType(rts, 0,SS_POINTER);
        ssSetDWorkComplexSignal(rts, 0, 0);
        ssSetDWork(rts, 0, &asyncip_sl_1_sm_ip_test_DWork.OpIPSocketCtrl1_PWORK);
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
void asyncip_sl_1_sm_ip_test_terminate(void)
{
  /* Level2 S-Function Block: '<S5>/S-Function2' (sfun_send_async) */
  {
    SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[0];
    sfcnTerminate(rts);
  }

  /* Level2 S-Function Block: '<S3>/S-Function1' (sfun_recv_async) */
  {
    SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[1];
    sfcnTerminate(rts);
  }

  /* Level2 S-Function Block: '<S7>/S-Function' (OP_SEND) */
  {
    SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[2];
    sfcnTerminate(rts);
  }

  /* Level2 S-Function Block: '<S2>/OpIPSocketCtrl1' (sfun_gen_async_ctrl) */
  {
    SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[3];
    sfcnTerminate(rts);
  }
}

/*========================================================================*
 * Start of GRT compatible call interface                                 *
 *========================================================================*/
void MdlOutputs(int_T tid)
{
  asyncip_sl_1_sm_ip_test_output(tid);
}

void MdlUpdate(int_T tid)
{
  asyncip_sl_1_sm_ip_test_update(tid);
}

void MdlInitializeSizes(void)
{
  asyncip_sl_1_sm_ip_test_M->Sizes.numContStates = (0);/* Number of continuous states */
  asyncip_sl_1_sm_ip_test_M->Sizes.numY = (0);/* Number of model outputs */
  asyncip_sl_1_sm_ip_test_M->Sizes.numU = (0);/* Number of model inputs */
  asyncip_sl_1_sm_ip_test_M->Sizes.sysDirFeedThru = (0);/* The model is not direct feedthrough */
  asyncip_sl_1_sm_ip_test_M->Sizes.numSampTimes = (2);/* Number of sample times */
  asyncip_sl_1_sm_ip_test_M->Sizes.numBlocks = (12);/* Number of blocks */
  asyncip_sl_1_sm_ip_test_M->Sizes.numBlockIO = (8);/* Number of block outputs */
  asyncip_sl_1_sm_ip_test_M->Sizes.numBlockPrms = (249);/* Sum of parameter "widths" */
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
  asyncip_sl_1_sm_ip_test_DWork.SFunction_PreviousInput =
    asyncip_sl_1_sm_ip_test_P.SFunction_X0;

  /* Level2 S-Function Block: '<S5>/S-Function2' (sfun_send_async) */
  {
    SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[0];
    sfcnInitializeConditions(rts);
    if (ssGetErrorStatus(rts) != (NULL))
      return;
  }

  /* Level2 S-Function Block: '<S3>/S-Function1' (sfun_recv_async) */
  {
    SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[1];
    sfcnInitializeConditions(rts);
    if (ssGetErrorStatus(rts) != (NULL))
      return;
  }

  /* Level2 S-Function Block: '<S7>/S-Function' (OP_SEND) */
  {
    SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[2];
    sfcnInitializeConditions(rts);
    if (ssGetErrorStatus(rts) != (NULL))
      return;
  }

  /* Level2 S-Function Block: '<S2>/OpIPSocketCtrl1' (sfun_gen_async_ctrl) */
  {
    SimStruct *rts = asyncip_sl_1_sm_ip_test_M->childSfunctions[3];
    sfcnInitializeConditions(rts);
    if (ssGetErrorStatus(rts) != (NULL))
      return;
  }
}

void MdlStart(void)
{
  /* Start for DiscretePulseGenerator: '<S2>/data ready 1 kHz' */
  asyncip_sl_1_sm_ip_test_DWork.clockTickCounter = 0;

  /* Start for DiscretePulseGenerator: '<S2>/Pulse Generator' */
  asyncip_sl_1_sm_ip_test_DWork.clockTickCounter_c = 0;
  MdlInitialize();
}

void MdlTerminate(void)
{
  asyncip_sl_1_sm_ip_test_terminate();
}

RT_MODEL_asyncip_sl_1_sm_ip_test *asyncip_sl_1_sm_ip_test(void)
{
  asyncip_sl_1_sm_ip_test_initialize(1);
  return asyncip_sl_1_sm_ip_test_M;
}

/*========================================================================*
 * End of GRT compatible call interface                                   *
 *========================================================================*/
