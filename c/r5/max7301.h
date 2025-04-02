//
// R5 integrated controller 
// ZCU102 + ADI CN0585/CN0584
//
// IRQs from PL and 
// interproc communications with A53/linux
//
//////////////////////////////////////////
//
// this is the setup of the MAX7301 on ADI CN0585
// it's a GPIO that enables/disables ADC and DAC channels
// It's interfaced via a Xilinx standard AXI (quad) SPI IP in PL
//

#ifndef MAX7301_H_
#define MAX7301_H_

#include "xparameters.h"
#include "xspi.h"
#include "xspi_l.h"
#include "xil_printf.h"
#include "common.h"

// AXI SPI has base address 0x8003_0000,  which is XPAR_MAX7301_SPI_0_BASEADDR in xparameters.h
#define MAX7301_XSPI_BADDR     XPAR_MAX7301_SPI_0_BASEADDR

// ##########  types  #######################

// ##########  extern globals  ################

// ##########  protos  ########################
int InitMAX7301(void);



#endif
