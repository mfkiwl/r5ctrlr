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
#define RPMSGCMD_NOP                 0
#define RPMSGCMD_WRITE_DAC           1
#define RPMSGCMD_READ_DAC            2
#define RPMSGCMD_WRITE_DACCH         3
#define RPMSGCMD_READ_ADC            4
#define RPMSGCMD_WRITE_FSAMPL        5
#define RPMSGCMD_READ_FSAMPL         6
#define RPMSGCMD_WGEN_ONOFF          7
#define RPMSGCMD_READ_STATE          8
#define RPMSGCMD_WRITE_WGEN_CH_CONF  9
#define RPMSGCMD_READ_WGEN_CH_CONF  10
#define RPMSGCMD_WRITE_WGEN_CH_EN   11
#define RPMSGCMD_READ_WGEN_CH_EN    12
#define RPMSGCMD_RESET              13
#define RPMSGCMD_WRITE_TRIG         14
#define RPMSGCMD_READ_TRIG          15
#define RPMSGCMD_WRITE_TRIG_CFG     16
#define RPMSGCMD_READ_TRIG_CFG      17
#define RPMSGCMD_WRITE_DACOFFS      18
#define RPMSGCMD_READ_DACOFFS       19

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
#define WGEN_CH_ENABLE_OFF       0
#define WGEN_CH_ENABLE_ON        1
#define WGEN_CH_ENABLE_SINGLE    2
#define WGEN_CH_TYPE_DC          0
#define WGEN_CH_TYPE_SINE        1
#define WGEN_CH_TYPE_SWEEP       2

// DAC output select
#define OUTPUT_SELECT_WGEN   0
#define OUTPUT_SELECT_CTRLR  1

// ##########  types  #######################

typedef struct
  {
  int    enable;   // OFF/ON/SINGLE
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
  // user coeff
  double a[3]; // x_i coeff
  double b[2]; // y_i coeff
  // internal "static" vars
  double x[2]; // x pipeline
  double y[2]; // y pipeline
  } IIR2_COEFF;

typedef struct
  {
  // user gains
  double Gp;         // prop gain
  double Gi;         // integr gain
  double G1d;        // deriv gain #1
  double G2d;        // deriv gain #2
  double G_aiw;      // anti integral windup gain
  double out_sat;    // output saturation limit
  double in_thr;     // input dead band
  bool deriv_on_PV;  // derivative on process variable?
  bool invert_cmd;   // invert commanded value
  bool invert_meas;  // invert measured value
  // internal "static" vars
  double xn1;        // x(n-1)
  double yi_n1;      // integral part at previous step = y_I(n-1)
  double yd_n1;      // derivative part at previous step = y_D(n-1)
  } PID_GAINS;

typedef struct
  {
  int inputSelect;
  double input_MISO_A[5], input_MISO_B[5],
         input_MISO_C[5], input_MISO_D[5],
         output_MISO_E[5], output_MISO_F[5];
  PID_GAINS  PID1, PID2;
  IIR2_COEFF IIR1, IIR2;
  } CTRLLOOP_CH_CONFIG;


typedef struct rpmsg_endpoint RPMSG_ENDP_TYPE;

typedef struct
  {
  u32 command;
  u32 option;
  u32 data[122];
  } R5_RPMSG_TYPE;



#endif
