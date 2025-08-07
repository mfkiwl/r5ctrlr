//
// R5 integrated controller 
// ZCU102 + ADI CN0585/CN0584
//
// IRQs from PL and 
// interproc communications with A53/linux
//
//////////////////////////////////////////
//
// this is the management of the two dual DACs AD3552 on ADI CN0585
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
#define DAC_A_BADDR     XPAR_AD3552_SPI_A_BASEADDR
#define DAC_B_BADDR     XPAR_AD3552_SPI_B_BASEADDR

#define MAX_READ_RETRIES     10
#define RESET_WAIT          10000UL

// DAC register numbers
#define DAC_STATUS_WORD 0
#define DAC_CTRL_WORD   1
#define DAC_TRANSACT    2

// DAC STATUS register constants
#define DAC_SPI_ERRCODE_MASK    0x0000000F
#define DAC_SPI_NOERR           0
#define DAC_SPI_ERR_RW_MISMATCH 1
#define DAC_SPI_ERR_OVERRUN     2
#define DAC_SPI_ERR_ZERO_BYTES  3
#define DAC_SPI_ERR_SPI_BUSY    4
// other err codes
#define DAC_SPI_ERR_RD_TIMEOUT  16

// DAC CTRL register constants
#define SPI_CLK_NO_DIVIDER      0x00000000
#define SPI_CLK_DIVIDE_BY_4     0x0000000C
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
#define DAC_DATA_MASK                0x00FFFFFF
#define DAC_ADDR_READ_TRANSACT       0x00000080

// AD3552 DAC internal register address
#define AD3552_INTERFACE_CONFIG_A      0x00
#define AD3552_SCRATCHPAD              0x0A
#define AD3552_STREAM_MODE             0x0E
#define AD3552_TRANSFER_REGISTER       0x0F
#define AD3552_CH0_CH1_OUTPUT_RANGE    0x19
#define AD3552_SW_LDAC_24B             0x45

// AD3552 scaling
#define AD3552_AMPL                    (32768.0)
#define AD3552_OFFS                    (32768.0)
#define AD3552_FULLSCALE_VOLT          (10.0)
#define AD3552_MAX_NORMALIZED          (32767.0/32768.0)

// ##########  types  #######################

// ##########  extern globals  ################

// ##########  protos  ########################
int InitAD3552(void);
int WriteDacRegister(int DACindex, u32 addr, u8 data);
int ReadDacRegister(int DACindex, u32 addr, u8* dataptr);
int WriteDacSamples(int DACindex, u16 ch0data, u16 ch1data);
int UpdateDacOutput(int DACindex);

#endif
