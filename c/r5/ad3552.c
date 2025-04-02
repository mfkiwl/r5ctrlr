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
  int i, status;
  u32 theval;

  // set CLK divider=2 and perform both HW and SW reset
  *(DAC_BADDR[0]+CTRL_WORD) = SPI_CLK_DIVIDE_BY_2 | DAC_HW_RESET | DAC_SOFT_RESET;
  // remove HW reset
  *(DAC_BADDR[0]+CTRL_WORD) = SPI_CLK_DIVIDE_BY_2;

  // test SPI WRITE
  *(DAC_BADDR[0]+DAC_TRANSACT) = DAC_WRITE_TRANSACTION |
                                 DAC_ADDRESS_PHASE |
                                 DAC_1_BYTE_TRANSACTION |
                                 0xA5;


  // test SPI READ

  theval = *(DAC_BADDR[0]+STATUS_WORD);
  theval &= DAC_SPI_ERRCODE_MASK;
  
  return XST_SUCCESS;
  }