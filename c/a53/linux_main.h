//
// R5 demo application
// IRQs from PL and 
// interproc communications with A53/linux
//
// this is the linux/A53 side
//

#ifndef MAIN_H_
#define MAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <openamp/open_amp.h>
#include <metal/alloc.h>
#include <metal/sys.h>
#include "platform.h"
#include "rproc.h"
#include <string.h>
#include "common.h"
#include "sample_shmem.h"
#include "SCPIserver.h"

#define IPI_DEV_NAME        "ff340000.ipi"
#define IPI_CHN_BITMASK     0x00000100
#define DEV_BUS_NAME        "platform" 
#define SHM_DEV_NAME        "3ed40000.shm"
#define RSC_MEM_PA          0x3ED48000UL
#define RSC_MEM_SIZE        0x2000UL

#define RPMSG_ANSWER_TIMEOUT_SEC (1.0)
#define RPMSG_ANSWER_VALID     0
#define RPMSG_ANSWER_ERR       1
#define RPMSG_ANSWER_TIMEOUT   2

// ##########  extern globals  ################
extern void *gplatform;
extern s16 g_adcval[4], g_dacval[4];


// ##########  protos  ########################

static int rpmsg_endpoint_cb(struct rpmsg_endpoint *ept, void *data, size_t len,
    uint32_t src, void *priv);
static void rpmsg_service_unbind(struct rpmsg_endpoint *ept);
static void rpmsg_name_service_bind_cb(struct rpmsg_device *rdev,
    const char *name, uint32_t dest);
int SetupSystem(void **platformp);
int CleanupSystem(void *platform);
static struct remoteproc *SetupRpmsg(int proc_index, int rsc_index);
int WaitForRpmsg(void);
void FlushRpmsg(void);
int main(int argc, char *argv[]);


#endif
