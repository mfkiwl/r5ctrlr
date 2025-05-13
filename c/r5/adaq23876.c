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

#include "adaq23876.h"

// ##########  globals  #######################

// ##########  implementation  ################

// s16 means signed int 16
void ReadADCs(s16 *ptr)
  {
  u32 x;
  x=*((volatile u32 *)XPAR_QUAD_ADAQ23876_0_BASEADDR+ADC_chan_B_A);
  *ptr     =      x  & 0x0000FFFF;
  *(ptr+1) = (x>>16) & 0x0000FFFF;
  x=*((volatile u32 *)XPAR_QUAD_ADAQ23876_0_BASEADDR+ADC_chan_D_C);
  *(ptr+2) =      x  & 0x0000FFFF;
  *(ptr+3) = (x>>16) & 0x0000FFFF;
  }

// -----------------------------------------------------------


