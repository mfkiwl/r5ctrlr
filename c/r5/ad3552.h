//
// R5 integrated controller 
// ZCU102 + ADI CN0585/CN0584
//
// IRQs from PL and 
// interproc communications with A53/linux
//
//////////////////////////////////////////
//
// this is the setup of the two dual DACs AD3552 on ADI CN0585
// It's interfaced via our custom IP in PL
//

#ifndef AD3552_H_
#define AD3552_H_

#include "xparameters.h"
#include "xil_printf.h"
#include "xstatus.h"
#include "common.h"

// In the PL there are two instances of our custom AD3552 SPI IP, 
// one for each DAC on ADI CN0585 board
// Here I define convenient constants based on the ones defined by the platform in xparameters.h
// AD3552 instance 1 has base address 0x8004_0000,  which is XPAR_AD3552_SPI_0_BASEADDR in xparameters.h
#define NUM_DACS        2
#define DAC_A_BADDR     XPAR_AD3552_SPI_0_BASEADDR
//#define DAC_B_BADDR     XPAR_AD3552_SPI_1_BASEADDR

// DAC register numbers
#define STATUS_WORD     0
#define CTRL_WORD       1
#define DAC_TRANSACT    2

// DAC STATUS register constants
#define DAC_SPI_ERRCODE_MASK    0x0000000F
#define DAC_SPI_NOERR           0
#define DAC_SPI_ERR_RW_MISMATCH 1
#define DAC_SPI_ERR_OVERRUN     2
#define DAC_SPI_ERR_ZERO_BYTES  3
#define DAC_SPI_ERR_SPI_BUSY    4

// DAC CTRL register constants
#define SPI_CLK_DIVIDE_BY_2     0x00000000
#define SPI_CLK_DIVIDE_BY_4     0x00000004
#define SPI_CLK_DIVIDE_BY_8     0x00000008
#define SPI_CLK_DIVIDE_BY_16    0x0000000C
#define DAC_HW_RESET            0x00000002
#define DAC_SOFT_RESET          0x00000001

// DAC TRANSACTION register constants
#define DAC_TRANSACTION_ENDED_MASK   0x80000000
#define DAC_READ_TRANSACTION         0x40000000
#define DAC_WRITE_TRANSACTION        0x00000000
#define DAC_ADDRESS_PHASE            0x20000000
#define DAC_DATA_PHASE               0x00000000
#define DAC_LAST_TRANSACTION         0x10000000
#define DAC_1_BYTE_TRANSACTION       0x02000000
#define DAC_2_BYTE_TRANSACTION       0x04000000
#define DAC_3_BYTE_TRANSACTION       0x06000000


// ##########  types  #######################

// ##########  extern globals  ################

// ##########  protos  ########################
int InitAD3552(void);



#endif
