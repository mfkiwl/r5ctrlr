//
// R5 integrated controller 
// ZCU102 + ADI CN0585/CN0584
//
// IRQs from PL and 
// interproc communications with A53/linux
//
//////////////////////////////////////////
//
// this is the management of the four ADCs ADAQ23876 on ADI CN0585
// It's interfaced via our custom IP in PL
//

#ifndef ADAQ23876_H_
#define ADAQ23876_H_

#include "xparameters.h"
#include "xil_printf.h"
#include "xstatus.h"
#include "common.h"

// In the PL there is just one instance of our custom QUAD_ADAQ23876 SPI IP, 
// which manages all the four ADCs on ADI CN0585 board
// Here I define convenient constants based on the ones defined by the platform in xparameters.h
// QUAD_ADAQ23876 instance has base address 0x8007_0000,
// which is XPAR_QUAD_ADAQ23876_0_BASEADDR in xparameters.h
#define ADC_BADDR     XPAR_QUAD_ADAQ23876_0_BASEADDR

// ADC register numbers
#define ADC_STATUS_WORD 0
#define ADC_CTRL_WORD   1
#define ADC_chan_B_A    2
#define ADC_chan_D_C    3

// ADAQ23876 scaling
#define ADAQ23876_FULLSCALE_CNT     (32768.0)
#define ADAQ23876_FULLSCALE_VOLT       (10.0)

// ##########  types  #######################

// ##########  extern globals  ################

// ##########  protos  ########################
// s16 means signed int 16
void ReadADCs(s16 *ptr);

#endif
