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
#define DECIMALS(x,n) (int)(fabs(x-(int)(x))*pow(10,n)+0.5)

//---------- openamp stuff -------------------------
#define RPMSG_SERVICE_NAME         "rpmsg-uopenamp-loop-params"

// commands from linux to R5
#define RPMSGCMD_NOP        0
#define RPMSGCMD_WRITE_DAC  1
#define RPMSGCMD_READ_DAC   2
#define RPMSGCMD_READ_ADC   3


// ##########  types  #######################
// typedef struct
//   {
//   float freqHz;
//   int   percentAmplitude;
//   float constValVolt;
//   } LOOP_PARAM_MSG_TYPE;

typedef struct rpmsg_endpoint RPMSG_ENDP_TYPE;

typedef struct
  {
  u32 command;
  u32 option;
  u32 data[122];
  } R5_RPMSG_TYPE;



#endif
