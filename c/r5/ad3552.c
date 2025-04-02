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

#include "ad3552.h"

// ##########  globals  #######################

volatile u32 *DAC_BADDR[] = {(volatile u32 *)DAC_A_BADDR};
// volatile u32 *DAC_BADDR[] = {(volatile u32 *)DAC_A_BADDR, (volatile u32 *)DAC_B_BADDR};


// ##########  implementation  ################

int InitAD3552(void)
  {
  int i, status, retries;
  u32 theval, theerr;

  // set CLK divider=2 and perform both HW and SW reset
  *(DAC_BADDR[0]+CTRL_WORD) = SPI_CLK_DIVIDE_BY_2 | DAC_HW_RESET | DAC_SOFT_RESET;
  // remove HW reset
  *(DAC_BADDR[0]+CTRL_WORD) = SPI_CLK_DIVIDE_BY_2;

  // test SPI WRITE
//  *(DAC_BADDR[0]+DAC_TRANSACT) = DAC_WRITE_TRANSACTION |
//                                 DAC_ADDRESS_PHASE |
//                                 DAC_1_BYTE_TRANSACTION |
//                                 0x36;
//
//  *(DAC_BADDR[0]+DAC_TRANSACT) = DAC_WRITE_TRANSACTION |
//                                 DAC_DATA_PHASE |
//                                 DAC_1_BYTE_TRANSACTION |
//                                 0xF1;
//
//  *(DAC_BADDR[0]+DAC_TRANSACT) = DAC_WRITE_TRANSACTION |
//                                 DAC_DATA_PHASE |
//                                 DAC_LAST_TRANSACTION |
//                                 DAC_1_BYTE_TRANSACTION |
//                                 0x2C;

  // test SPI READ
  *(DAC_BADDR[0]+DAC_TRANSACT) = DAC_WRITE_TRANSACTION |
                                 DAC_ADDRESS_PHASE |
                                 DAC_1_BYTE_TRANSACTION |
                                 0xC7;

  *(DAC_BADDR[0]+DAC_TRANSACT) = DAC_READ_TRANSACTION |
                                 DAC_DATA_PHASE |
                                 DAC_1_BYTE_TRANSACTION;

  *(DAC_BADDR[0]+DAC_TRANSACT) = DAC_READ_TRANSACTION |
                                 DAC_DATA_PHASE |
                                 DAC_LAST_TRANSACTION |
                                 DAC_2_BYTE_TRANSACTION;

  retries=MAX_READ_RETRIES;
  do
    {
    theval = *(DAC_BADDR[0]+DAC_TRANSACT);
    retries--;
    }
  while(((theval & DAC_TRANSACTION_ENDED_MASK)==0) && (retries>0));
  
  theerr = *(DAC_BADDR[0]+STATUS_WORD) & DAC_SPI_ERRCODE_MASK;

  if(retries>0)
    {
    LPRINTF("DAC SPI transaction completed; data: 0x%06X ; errcode: 0x%02X\n\r",
             theval & DAC_DATA_MASK,
             theerr);
    }
  else
    {
    LPRINTF("DAC SPI transaction TIMED OUT\n\r");
    }
  
  return XST_SUCCESS;
  }