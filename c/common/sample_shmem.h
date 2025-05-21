//
// R5 integrated controller 
// ZCU102 + ADI CN0585/CN0584
//
// IRQs from PL and 
// interproc communications with A53/linux
//
//////////////////////////////////////////
//
// this is the setup of a shared memory
// for ADC samples
//

#ifndef SAMPLE_SHMEM_H_
#define SAMPLE_SHMEM_H_

#include <unistd.h>
#include <metal/atomic.h>
#include <metal/io.h>
#include <metal/device.h>
#include <metal/sys.h>
#include <metal/irq.h>
#include <metal/alloc.h>
#include "common.h"

#ifdef ARMR5
// ########### R5 side
#include "xil_printf.h"
#include "xstatus.h"

#define SAMPLE_SHM_BUSNAME     "generic"
#else
// ########### linux side
#define SAMPLE_SHM_BUSNAME     "platform"
#endif

#define SAMPLE_SHM_DEVNAME     "3ed80000.shm"
#define SAMPLE_SHM_BADDR       0x3ED80000
#define SAMPLE_SHM_SIZE           0x20000

// ##########  types  #######################

// ##########  extern globals  ################
extern struct metal_device *sample_shmem_dev;
extern struct metal_io_region *sample_shmem_io;

// ##########  protos  ########################
int InitSampleShmem(void);

#endif
