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

// ##########  local defs  ###################

#ifdef ARMR5
// ########### R5 side
#define LPRINTF(format, ...) xil_printf(format, ##__VA_ARGS__)
#define LPERROR(format, ...) LPRINTF("ERROR: " format, ##__VA_ARGS__)
#else
// ########### linux side
#define LPRINTF(format, ...) printf(format, ##__VA_ARGS__)
#define LPERROR(format, ...) LPRINTF("ERROR: " format, ##__VA_ARGS__)
#endif

//---------- openamp stuff -------------------------
#define RPMSG_SERVICE_NAME         "rpmsg-uopenamp-loop-params"


#endif
