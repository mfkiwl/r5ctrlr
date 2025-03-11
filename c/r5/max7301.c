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

#include "max7301.h"

// ##########  globals  #######################

static XSpi  MAX7301_SpiInstance;
// XSPI driver API uses bytes, 
// but our SPI IP is configured for 16-bit transactions
#define MAX7301_COMMAND_WORDCOUNT 11
u16 MAX7301_Commands[MAX7301_COMMAND_WORDCOUNT]=
  {
  // write 0 to address 0x04 (config register): shutdown mode, no IRQ on change (this should be already be set by default)
  0x0400,
  // write 0x55 to address 0x09 (config P4..7): set P4..7 as outputs
  0x0955,
  // write 0xFF to address 0x0A (config P8..11): set P8..11 as inputs with pull-up
  0x0AFF,
  // write 0x55 to address 0x0B (config P12..15): set P12..15 as outputs; P12..14 are not used
  0x0B55,
  // write 0x55 to address 0x0C (config P16..19): set P16..19 as outputs; P19 is not used
  0x0C55,
  // write 0x55 to address 0x0D (config P20..23): set P20..23 as outputs even if they are not used; this minimizes quiescent current
  0x0D55,
  // write 0x55 to address 0x0E (config P24..27): set P24..27 as outputs even if they are not used; this minimizes quiescent current
  0x0E55,
  // write 0x55 to address 0x0F (config P28..31): set P28..31 as outputs even if they are not used; this minimizes quiescent current
  0x0F55,
  // write 0x0F to address 0x40 (register for ports 4-7): enable protector switches on CN0584
  0x400F,
  // write 0x00 (or 0x0F) to address 0x4F (register for ports 15-22): disable (or enable) the ADCs
  0x4F0F,
  // write 1 at address 0x04 (config register): take device out of shutdown and start normal operation
  0x0401
  };

// ##########  implementation  ################

int InitMAX7301(void)
  {
  int status;
  XSpi_Config *configPtr;
  
  configPtr = XSpi_LookupConfig(MAX7301_XSPI_BADDR);

  if(configPtr == NULL)
    {
    return XST_DEVICE_NOT_FOUND;
    }

  status = XSpi_CfgInitialize(&MAX7301_SpiInstance, configPtr, configPtr->BaseAddress);
  if(status != XST_SUCCESS)
    {
    return XST_FAILURE;
    }

  status = XSpi_SelfTest(&MAX7301_SpiInstance);
  if(status != XST_SUCCESS)
    {
    return XST_FAILURE;
    }

  // AXI SPI options: master, CPHA=0, CPOL=0, automatic slave select 
  status = XSpi_SetOptions(&MAX7301_SpiInstance, XSP_MASTER_OPTION );
  if(status != XST_SUCCESS)
    {
    return XST_FAILURE;
    }

  // we have one slave only
  status = XSpi_SetSlaveSelect(&MAX7301_SpiInstance, 1 );
  if(status != XST_SUCCESS)
    {
    return XST_FAILURE;
    }

  // start but disable the interrupts 
  // note that IRQ disable API must be called AFTER start API
  XSpi_Start(&MAX7301_SpiInstance);
  XSpi_IntrGlobalDisable(&MAX7301_SpiInstance);

  // send commands to MAX7301
  // without a FIFO in the SPI IP we cannot send the whole buffer in one call: 
  // we must transmit single words, so I make a loop
  //status = XSpi_Transfer(&MAX7301_SpiInstance, MAX7301_Commands, NULL, MAX7301_COMMAND_BYTECOUNT);
  for(int wordnum=0; wordnum<MAX7301_COMMAND_WORDCOUNT; wordnum++)
    {
    //LPRINTF("MAX7301 prog word #%d\r\n",wordnum+1);
    status = XSpi_Transfer(&MAX7301_SpiInstance, MAX7301_Commands+wordnum, NULL, 2);
    if(status != XST_SUCCESS)
      break;
    }

  if(status != XST_SUCCESS)
    {
    LPRINTF("Error in MAX7301 initialization.\r\n");
    return XST_FAILURE;
    }

    LPRINTF("MAX7301 successfully initialized\r\n");
    return XST_SUCCESS;
  }