//
// R5 demo application
// IRQs from PL and 
// interproc communications with A53/linux
//
// this file populates resource table 
// for BM remote for use by the Linux host
//
// this file is common to R5/baremetal and A53/linux
//

#ifndef COMMON_H_
#define COMMON_H_

#include <math.h>
#include <stdbool.h>
#include "doubledsplib.h"

// ##########  local defs  ###################

#ifdef ARMR5
// ########### R5 side
#define LPRINTF(format, ...) xil_printf(format, ##__VA_ARGS__)
#else
// ########### linux side
#define XST_SUCCESS                     0L
#define XST_FAILURE                     1L

typedef __uint32_t u32;
typedef    int16_t s16;

#define LPRINTF(format, ...) printf(format, ##__VA_ARGS__)
#endif

#define LPERROR(format, ...) LPRINTF("ERROR: " format, ##__VA_ARGS__)
#define SIGN(x) (x<0?'-':'+')
#define DECIMALS(x,n) (int)(fabs(x-(int)(x))*pow(10,n)+0.5)

//---------- openamp stuff -------------------------
#define RPMSG_SERVICE_NAME         "rpmsg-uopenamp-loop-params"

// frequency of the timer interrupt:
#define DEFAULT_TIMER_FREQ_HZ       10.e3

// commands from linux to R5
#define RPMSGCMD_NOP                        0
#define RPMSGCMD_WRITE_DAC                  1
#define RPMSGCMD_READ_DAC                   2
#define RPMSGCMD_WRITE_DACCH                3
#define RPMSGCMD_READ_ADC                   4
#define RPMSGCMD_WRITE_FSAMPL               5
#define RPMSGCMD_READ_FSAMPL                6
#define RPMSGCMD_WGEN_ONOFF                 7
#define RPMSGCMD_READ_STATE                 8
#define RPMSGCMD_WRITE_WGEN_CH_CONF         9
#define RPMSGCMD_READ_WGEN_CH_CONF         10
#define RPMSGCMD_WRITE_WGEN_CH_STATE       11
#define RPMSGCMD_READ_WGEN_CH_STATE        12
#define RPMSGCMD_RESET                     13
#define RPMSGCMD_WRITE_TRIG                14
#define RPMSGCMD_READ_TRIG                 15
#define RPMSGCMD_WRITE_TRIG_CFG            16
#define RPMSGCMD_READ_TRIG_CFG             17
#define RPMSGCMD_WRITE_DACOFFS             18
#define RPMSGCMD_READ_DACOFFS              19
#define RPMSGCMD_WRITE_ADCOFFS             20
#define RPMSGCMD_READ_ADCOFFS              21
#define RPMSGCMD_WRITE_ADCGAIN             22
#define RPMSGCMD_READ_ADCGAIN              23
#define RPMSGCMD_WRITE_DACOUTSEL           24
#define RPMSGCMD_READ_DACOUTSEL            25
#define RPMSGCMD_CTRLLOOP_ONOFF            26
#define RPMSGCMD_RESET_PID                 27
#define RPMSGCMD_RESET_IIR                 28
#define RPMSGCMD_WRITE_CTRLLOOP_CH_STATE   29
#define RPMSGCMD_READ_CTRLLOOP_CH_STATE    30
#define RPMSGCMD_WRITE_CTRLLOOP_CH_INSEL   31
#define RPMSGCMD_READ_CTRLLOOP_CH_INSEL    32
#define RPMSGCMD_WRITE_PID_GAINS           33
#define RPMSGCMD_READ_PID_GAINS            34
#define RPMSGCMD_WRITE_PID_THR             35
#define RPMSGCMD_READ_PID_THR              36
#define RPMSGCMD_WRITE_PID_PV_DERIV        37
#define RPMSGCMD_READ_PID_PV_DERIV         38
#define RPMSGCMD_WRITE_PID_INVCMD          39
#define RPMSGCMD_READ_PID_INVCMD           40
#define RPMSGCMD_WRITE_PID_INVMEAS         41
#define RPMSGCMD_READ_PID_INVMEAS          42
#define RPMSGCMD_WRITE_IIR_COEFF           43
#define RPMSGCMD_READ_IIR_COEFF            44
#define RPMSGCMD_WRITE_MATRIX_ROW          45
#define RPMSGCMD_READ_MATRIX_ROW           46

// R5 application state
#define R5CTRLR_IDLE     0
#define R5CTRLR_RECORD   1
#define R5CTRLR_WAVEGEN  2
#define R5CTRLR_CTRLLOOP 4

// recorder state
#define RECORDER_IDLE          0
#define RECORDER_FORCE         1
#define RECORDER_ARMED         2
#define RECORDER_ACQUIRING     3
// recorder mode
#define RECORDER_SWEEP         0
#define RECORDER_SLOPE         1
// recorder slope type
#define RECORDER_SLOPE_RISING  0
#define RECORDER_SLOPE_FALLING 1

// wave generator channel config
#define WGEN_CH_STATE_OFF       0
#define WGEN_CH_STATE_ON        1
#define WGEN_CH_STATE_SINGLE    2
#define WGEN_CH_TYPE_DC          0
#define WGEN_CH_TYPE_SINE        1
#define WGEN_CH_TYPE_SWEEP       2

// control loop
#define CTRLLOOP_CH_DISABLED 0
#define CTRLLOOP_CH_ENABLED  1

// DAC output select
#define OUTPUT_SELECT_WGEN   0
#define OUTPUT_SELECT_CTRLR  1

// ##########  types  #######################

typedef struct
  {
  int    state;   // OFF/ON/SINGLE
  int    type;     // DC/SINE/SWEEP
  float  ampl, offs, f1, f2, dt;
  } WAVEGEN_CH_CONFIG;

typedef struct
  {
  int    state;     // IDLE/FORCE/ARMED/ACQUIRING
  int    mode;      // SWEEP/SLOPE
  int    trig_chan; // in range [1,4]
  int    slopedir;  // RISING/FALLING
  float  level;
  } TRIG_CONFIG;

typedef struct
  {
  int state;         // ENABLED/DISABLED
  int inputSelect;
  float input_MISO_A[5], input_MISO_B[5],
        input_MISO_C[5], input_MISO_D[5],
        output_MISO_E[5], output_MISO_F[5];
  PID_GAINS  PID[2];
  IIR2_COEFF IIR[2];
  } CTRLLOOP_CH_CONFIG;


typedef struct rpmsg_endpoint RPMSG_ENDP_TYPE;

typedef struct
  {
  u32 command;
  u32 option;
  u32 data[122];
  } R5_RPMSG_TYPE;



#endif
