//
// R5 integrated controller 
// ZCU102 + ADI CN0585/CN0584
//
// IRQs from PL and 
// interproc communications with A53/linux
//
//////////////////////////////////////////
//
// this is the R5 main side
//

#include "r5_main.h"

//#define PROFILE
#define PRINTOUT

// ##########  globals  #######################

void *gplatform;
// openamp
static struct rpmsg_endpoint lept;
static int shutdown_req = 0;
struct rpmsg_device *rpdev;

// Polling information used by remoteproc operations.
static metal_phys_addr_t poll_phys_addr = POLL_BASE_ADDR;
struct metal_device ipi_device = 
  {
  .name = "poll_dev",
  .bus = NULL,
  .num_regions = 1,
  .regions = 
    {
      {
      .virt = (void *)POLL_BASE_ADDR,
      .physmap = &poll_phys_addr,
      .size = 0x1000,
      .page_shift = -1UL,
      .page_mask = -1UL,
      .mem_flags = DEVICE_NONSHARED | PRIV_RW_USER_RW,
      .ops = {NULL},
      }
    },
  .node = {NULL},
  .irq_num = 1,
  .irq_info = (void *)IPI_IRQ_VECT_ID,
  };

static struct remoteproc_priv rproc_priv =
  {
  .ipi_name = IPI_DEV_NAME,
  .ipi_bus_name = IPI_BUS_NAME,
  .ipi_chn_mask = IPI_CHN_BITMASK,
  };

static struct remoteproc rproc_inst;

// IRQ
XScuGic interruptController;
static XScuGic_Config *gicConfig;
unsigned long int irq_cntr[4];
unsigned long last_irq_cnt;
XGpio theGpio;
static XGpio_Config *gpioConfig;
XTmrCtr theTimer;
static XTmrCtr_Config *gTimerConfig;
float gTimerIRQlatency, gMaxLatency;
int gTimerIRQoccurred;

double gFsampl;
u16    dacval[4];
s16    adcval[4];
double g_x[4], g_y[4];
double g2pi, gPhase[4], gFreq[4];
int gR5ctrlState;
WAVEGEN_CH_CONFIG gWavegenChanConfig[4];
unsigned long gTotSweepSamples[4], gCurSweepSamples[4];

// table of execution times for profiling;
// entries are <x>, <x^2>, min, max, N_entries
double time_table[PROFILE_TIME_ENTRIES][5];


// ##########  implementation  ################

static int rpmsg_endpoint_cb(struct rpmsg_endpoint *ept, void *data, size_t len, 
                            uint32_t src, void *priv)
  {
  (void)priv;   // avoid warning on unused parameter
  (void)src;    // avoid warning on unused parameter
  (void)ept;    // avoid warning on unused parameter

  u32 cmd, d;
  int i, numbytes, rpmsglen, ret, nch;
  double dval;

  // update the total number of received messages, for debug purposes
  irq_cntr[IPI_CNTR]++;

  rpmsglen=sizeof(R5_RPMSG_TYPE);

  if(len<rpmsglen)
    {
    LPRINTF("incomplete message received.\n\r");
    return RPMSG_ERR_BUFF_SIZE;
    }

  cmd=((R5_RPMSG_TYPE*)data)->command;
  
  switch(cmd)
    {
    // update all DAC channels with the values requested by linux
    case RPMSGCMD_WRITE_DAC:
      d=((R5_RPMSG_TYPE*)data)->data[0];
      g_y[0]=((s16)(d&0x0000FFFF)) / AD3552_AMPL;
      g_y[1]=((s16)((d>>16)&0x0000FFFF)) / AD3552_AMPL;
      d=((R5_RPMSG_TYPE*)data)->data[1];
      g_y[2]=((s16)(d&0x0000FFFF)) / AD3552_AMPL;
      g_y[3]=((s16)((d>>16)&0x0000FFFF)) / AD3552_AMPL;
      break;

    // update only one DAC channel with the value requested by linux
    case RPMSGCMD_WRITE_DACCH:
      d=((R5_RPMSG_TYPE*)data)->data[0];
      dval=((s16)(d&0x0000FFFF)) / (1.0*AD3552_AMPL);
      nch=(d>>16)&0x0000FFFF;
      if((nch>=1)&&(nch<=4))
        g_y[nch-1]=dval;
      else
        return RPMSG_ERR_PARAM;
      break;

    // read back DAC values to linux
    case RPMSGCMD_READ_DAC:
      ((R5_RPMSG_TYPE*)data)->command = RPMSGCMD_READ_DAC;
      // need an intermediate cast to s32 for negative numbers
      ((R5_RPMSG_TYPE*)data)->data[0] = ((((u32)((s32)(round(g_y[1]*AD3552_AMPL))))<<16)&0xFFFF0000) | 
                                         (((u32)((s32)(round(g_y[0]*AD3552_AMPL))))&0x0000FFFF);
      ((R5_RPMSG_TYPE*)data)->data[1] = ((((u32)((s32)(round(g_y[3]*AD3552_AMPL))))<<16)&0xFFFF0000) | 
                                         (((u32)((s32)(round(g_y[2]*AD3552_AMPL))))&0x0000FFFF);

      numbytes= rpmsg_send(ept, data, rpmsglen);
      if(numbytes<rpmsglen)
        {
        // answer transmission incomplete
        LPRINTF("DAC READBACK incomplete answer transmitted.\n\r");
        return RPMSG_ERR_BUFF_SIZE;
        }
      break;

    // send current ADC values back to linux
    case RPMSGCMD_READ_ADC:
      ((R5_RPMSG_TYPE*)data)->command = RPMSGCMD_READ_ADC;
      ((R5_RPMSG_TYPE*)data)->data[0] = ((((u32)adcval[1])<<16)&0xFFFF0000) | (((u32)adcval[0])&0x0000FFFF);
      ((R5_RPMSG_TYPE*)data)->data[1] = ((((u32)adcval[3])<<16)&0xFFFF0000) | (((u32)adcval[2])&0x0000FFFF);

      numbytes= rpmsg_send(ept, data, rpmsglen);
      if(numbytes<rpmsglen)
        {
        // answer transmission incomplete
        LPRINTF("ADC READ incomplete answer transmitted.\n\r");
        return RPMSG_ERR_BUFF_SIZE;
        }
      break;

    // change sampling frequency
    case RPMSGCMD_WRITE_FSAMPL:
      d=((R5_RPMSG_TYPE*)data)->data[0];
      SetSamplingFreq(d);
      break;

    // send current sampling frequency
    case RPMSGCMD_READ_FSAMPL:
      ((R5_RPMSG_TYPE*)data)->command = RPMSGCMD_READ_FSAMPL;
      ((R5_RPMSG_TYPE*)data)->data[0] = (u32)gFsampl;

      numbytes= rpmsg_send(ept, data, rpmsglen);
      if(numbytes<rpmsglen)
        {
        // answer transmission incomplete
        LPRINTF("FSAMPL READ incomplete answer transmitted.\n\r");
        return RPMSG_ERR_BUFF_SIZE;
        }
      break;

    // start or stop WAVEFORM GENERATOR mode
    case RPMSGCMD_WGEN_ONOFF:
      d=((R5_RPMSG_TYPE*)data)->data[0];
      
      // reset sweep counters every time we get this command
      for(i=0; i<4; i++)
        {
        gPhase[i]=0.;
        gFreq[i]=0.;
        gTotSweepSamples[i]=0;
        gCurSweepSamples[i]=0;
        }
      
      if(d==0)
        gR5ctrlState=R5CTRLR_IDLE;
      else
        gR5ctrlState=R5CTRLR_WAVEGEN;
      break;

    // send current state
    case RPMSGCMD_READ_STATE:
      ((R5_RPMSG_TYPE*)data)->command = RPMSGCMD_READ_STATE;
      ((R5_RPMSG_TYPE*)data)->data[0] = (u32)gR5ctrlState;

      numbytes= rpmsg_send(ept, data, rpmsglen);
      if(numbytes<rpmsglen)
        {
        // answer transmission incomplete
        LPRINTF("STATE READ incomplete answer transmitted.\n\r");
        return RPMSG_ERR_BUFF_SIZE;
        }
      break;

    // config one wavefrom generator channel
    case RPMSGCMD_WRITE_WGEN_CH_CONF:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      
      gWavegenChanConfig[nch-1].type   = (int)(((R5_RPMSG_TYPE*)data)->data[1]);
      // read floating point values directly as float (32 bit)
      memcpy(&(gWavegenChanConfig[nch-1].ampl), &(((R5_RPMSG_TYPE*)data)->data[2]), sizeof(u32));
      memcpy(&(gWavegenChanConfig[nch-1].offs), &(((R5_RPMSG_TYPE*)data)->data[3]), sizeof(u32));
      memcpy(&(gWavegenChanConfig[nch-1].f1),   &(((R5_RPMSG_TYPE*)data)->data[4]), sizeof(u32));
      memcpy(&(gWavegenChanConfig[nch-1].f2),   &(((R5_RPMSG_TYPE*)data)->data[5]), sizeof(u32));
      memcpy(&(gWavegenChanConfig[nch-1].dt),   &(((R5_RPMSG_TYPE*)data)->data[6]), sizeof(u32));

      // reset sweep counters every time we get this command
      for(i=0; i<4; i++)
        {
        gPhase[i]=0.;
        gFreq[i]=0.;
        gTotSweepSamples[i]=0;
        gCurSweepSamples[i]=0;
        }

      break;
    
    // read back config of one wavefrom generator channel
    case RPMSGCMD_READ_WGEN_CH_CONF:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      
      ((R5_RPMSG_TYPE*)data)->command = RPMSGCMD_READ_WGEN_CH_CONF;
      ((R5_RPMSG_TYPE*)data)->data[0] = (u32)nch;
      ((R5_RPMSG_TYPE*)data)->data[1] = (u32)(gWavegenChanConfig[nch-1].type);
      // write floating point values directly as float (32 bit)
      memcpy(&(((R5_RPMSG_TYPE*)data)->data[2]), &(gWavegenChanConfig[nch-1].ampl), sizeof(u32));
      memcpy(&(((R5_RPMSG_TYPE*)data)->data[3]), &(gWavegenChanConfig[nch-1].offs), sizeof(u32));
      memcpy(&(((R5_RPMSG_TYPE*)data)->data[4]), &(gWavegenChanConfig[nch-1].f1),   sizeof(u32));
      memcpy(&(((R5_RPMSG_TYPE*)data)->data[5]), &(gWavegenChanConfig[nch-1].f2),   sizeof(u32));
      memcpy(&(((R5_RPMSG_TYPE*)data)->data[6]), &(gWavegenChanConfig[nch-1].dt),   sizeof(u32));

      numbytes= rpmsg_send(ept, data, rpmsglen);
      if(numbytes<rpmsglen)
        {
        // answer transmission incomplete
        LPRINTF("WGEN CH CONF READ incomplete answer transmitted.\n\r");
        return RPMSG_ERR_BUFF_SIZE;
        }
      break;
    
    // enable/disable one wavefrom generator channel
    case RPMSGCMD_WRITE_WGEN_CH_EN:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      
      // reset sweep counters every time we get this command
      for(i=0; i<4; i++)
        {
        gPhase[i]=0.;
        gFreq[i]=0.;
        gTotSweepSamples[i]=0;
        gCurSweepSamples[i]=0;
        }

      gWavegenChanConfig[nch-1].enable   = (int)(((R5_RPMSG_TYPE*)data)->data[1]);
      break;

    // read back on/off state of one wavefrom generator channel
    case RPMSGCMD_READ_WGEN_CH_EN:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      
      ((R5_RPMSG_TYPE*)data)->command = RPMSGCMD_READ_WGEN_CH_EN;
      ((R5_RPMSG_TYPE*)data)->data[0] = (u32)nch;
      ((R5_RPMSG_TYPE*)data)->data[1] = (u32)(gWavegenChanConfig[nch-1].enable);

      numbytes= rpmsg_send(ept, data, rpmsglen);
      if(numbytes<rpmsglen)
        {
        // answer transmission incomplete
        LPRINTF("WGEN CH EN READ incomplete answer transmitted.\n\r");
        return RPMSG_ERR_BUFF_SIZE;
        }
      break;

    // reset
    case RPMSGCMD_RESET:
      InitVars();
      SetSamplingFreq((u32)gFsampl);
      break;

    }

  return RPMSG_SUCCESS;
}


// -----------------------------------------------------------

static void rpmsg_service_unbind(struct rpmsg_endpoint *ept)
  {
  (void)ept;   // avoid warning on unused parameter
  LPRINTF("RPMSG channel close requested\n\r");
  shutdown_req = 1;
  }

// -----------------------------------------------------------

static void AssertPrint(const char8 *FilenamePtr, s32 LineNumber)
  {
  LPRINTF("ASSERT: File Name: %s ", FilenamePtr);
  LPRINTF("Line Number: %ld\n\r", LineNumber);
  }


// -----------------------------------------------------------

void LocalAbortHandler(void *callbackRef)
  {
  (void)callbackRef;   // avoid warning on unused parameter

  // Data Abort exception handler
  LPRINTF("DATA ABORT exception\n\r");
  }


// -----------------------------------------------------------

void LocalUndefinedHandler(void *callbackRef)
  {
  (void)callbackRef;   // avoid warning on unused parameter

  // Undefined Instruction exception handler
  LPRINTF("UNDEFINED INSTRUCTION exception\n\r");
  }


// -----------------------------------------------------------

void RegbankISR(void *callbackRef)
  {
  //unsigned int thereg
  unsigned int theval;
  
  (void)callbackRef;   // avoid warning on unused parameter

  // Regbank Interrupt Servicing Routine
  irq_cntr[REGBANK_IRQ_CNTR]++;
  // don't use printf in ISR! Xilinx standalone stdio is NOT thread safe
  //printf("IRQ received from REGBANK (%lu)\n\r", irq_cntr[REGBANK_IRQ_CNTR]);
  // manually reset IRQ bit in regbank
  theval=*(REGBANK+0);
  theval&=0xFFFFFFFE;
  *(REGBANK+0)=theval;
  }


// -----------------------------------------------------------

void GpioISR(void *callbackRef)
  {
  XGpio *gpioPtr = (XGpio *)callbackRef;
  
  // AXI GPIO Interrupt Servicing Routine
  irq_cntr[GPIO_IRQ_CNTR]++;
  // don't use printf in ISR! Xilinx standalone stdio is NOT thread safe
  //printf("IRQ received from AXI GPIO (%lu)\n\r", irq_cntr[GPIO_IRQ_CNTR]);

  XGpio_InterruptClear(gpioPtr, GPIO_BUTTON_IRQ_MASK);
  }


// -----------------------------------------------------------

static inline double GetTimer_us(void)
  {
  u32 loadReg, cnts;
  cnts=XTmrCtr_GetValue(&theTimer, TIMER_NUMBER);
  loadReg=XTmrCtr_ReadReg(theTimer.BaseAddress, TIMER_NUMBER, XTC_TLR_OFFSET);
  return 1.e6*(loadReg - cnts)/(1.0*gTimerConfig->SysClockFreqHz);
  }

// -----------------------------------------------------------
/*
static void TimerISR(void *callbackRef, u8 timer_num)
  {
  XTmrCtr *timerPtr = (XTmrCtr *)callbackRef;
  u32 cnts;

  // AXI timer Interrupt Servicing Routine

  // read current counter value to measure latency
  cnts=XTmrCtr_GetValue(timerPtr, timer_num);
  gTimerIRQlatency=(gTimerConfig->SysClockFreqHz/gFsampl - cnts*1.0)/gTimerConfig->SysClockFreqHz;

  // increment number of received timer interrupts
  irq_cntr[TIMER_IRQ_CNTR]++;
 
  // update max latency value only after initial transient
  if(irq_cntr[TIMER_IRQ_CNTR]>1)
    {
    if(gTimerIRQlatency>gMaxLatency)
      gMaxLatency=gTimerIRQlatency;
    }

  // don't use printf in ISR! Xilinx standalone stdio is NOT thread safe
  //printf("IRQ received from AXI timer (%lu)\n\r", ++irq_cntr[TIMER_IRQ_CNTR]);
  }
*/

void FiqHandler(void *cb)
  {
  XTmrCtr *timerPtr = (XTmrCtr *)cb;
  u32 controlStatusReg, loadReg, cnts;

  //printf("FIQ\n\r");

  // read current counter value to measure latency
  gTimerIRQlatency=GetTimer_us();

  // clear AXI timer IRQ bit to acknowledge interrupt
  controlStatusReg = XTmrCtr_ReadReg(timerPtr->BaseAddress, TIMER_NUMBER, XTC_TCSR_OFFSET);
  XTmrCtr_WriteReg(timerPtr->BaseAddress, TIMER_NUMBER, XTC_TCSR_OFFSET, controlStatusReg | XTC_CSR_INT_OCCURED_MASK);

  // increment number of received timer interrupts
  irq_cntr[TIMER_IRQ_CNTR]++;

  // update max latency value only after initial transient
  if(irq_cntr[TIMER_IRQ_CNTR]>1)
    {
    if(gTimerIRQlatency>gMaxLatency)
      gMaxLatency=gTimerIRQlatency;
    }

  // signal main loop that a new timer IRQ occurred
  gTimerIRQoccurred++;
  }


// -----------------------------------------------------------

int SetupIRQs(void)
  {
  int status;
  u8  irqPriority, irqSensitivity;
  uint32_t int_id;
  uint32_t mask_cpu_id = ((u32)0x1 << XPAR_CPU_ID);
  uint32_t target_cpu;
  u32 reg;

  mask_cpu_id |= mask_cpu_id << 8U;
  mask_cpu_id |= mask_cpu_id << 16U;

  Xil_ExceptionDisable();

  // setup IRQ system --------------------------

  gicConfig=XScuGic_LookupConfig(INTC_DEVICE_ID);
  if(NULL==gicConfig) 
    return XST_FAILURE;

  status=XScuGic_CfgInitialize(&interruptController, gicConfig, gicConfig->CpuBaseAddress);
  if (status!=XST_SUCCESS)
    return XST_FAILURE;

  status=XScuGic_SelfTest(&interruptController);
  if(status!=XST_SUCCESS)
    return XST_FAILURE;

  // map IPIs for openAmp
  for (int_id = 32U; int_id<XSCUGIC_MAX_NUM_INTR_INPUTS;int_id=int_id+4U)
    {
    target_cpu = XScuGic_DistReadReg(&interruptController, XSCUGIC_SPI_TARGET_OFFSET_CALC(int_id));
    // Remove current CPU from interrupt target register
    target_cpu &= ~mask_cpu_id;
    XScuGic_DistWriteReg(&interruptController, XSCUGIC_SPI_TARGET_OFFSET_CALC(int_id), target_cpu);
    }
  XScuGic_InterruptMaptoCpu(&interruptController, XPAR_CPU_ID, IPI_IRQ_VECT_ID);

  Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
                               (Xil_ExceptionHandler) XScuGic_InterruptHandler,
                               &interruptController);
  XScuGic_Disable(&interruptController, IPI_IRQ_VECT_ID);
  //Xil_ExceptionEnable();

  // now register the IRQs I am interested in ----------------------------

  // openamp IPI ------------------------------------------
  XScuGic_Connect(&interruptController, IPI_IRQ_VECT_ID,
                  (Xil_ExceptionHandler)metal_xlnx_irq_isr,
                  (void *)IPI_IRQ_VECT_ID);
  status= metal_xlnx_irq_init();
  if(status!=XST_SUCCESS)
    return XST_FAILURE;

  // Setup PL REGBANK IRQ ---------------------------------
  // register ISR
  status=XScuGic_Connect(&interruptController, INTC_REGBANK_IRQ_ID,
                         (Xil_ExceptionHandler)RegbankISR,
                         (void *)&interruptController);
  if(status!=XST_SUCCESS)
    return XST_FAILURE;
  // set priority and endge sensitivity
  XScuGic_GetPriorityTriggerType(&interruptController, INTC_REGBANK_IRQ_ID, &irqPriority, &irqSensitivity);
  //printf("REGBANK old priority=0x%02X; old sensitivity=0x%02X\n\r", irqPriority, irqSensitivity);
  //irqPriority = 0;           // in step of 8; 0=highest; 248=lowest
  irqSensitivity=0x03;         // rising edge
  XScuGic_SetPriorityTriggerType(&interruptController, INTC_REGBANK_IRQ_ID, irqPriority, irqSensitivity);
  // now enable the IRQ
  XScuGic_Enable(&interruptController, INTC_REGBANK_IRQ_ID);


  // Setup AXI GPIO IRQs ---------------------------------
  // register ISR
  status=XScuGic_Connect(&interruptController, INTC_AXIGPIO_IRQ_ID,
                         (Xil_ExceptionHandler)GpioISR,
                         (void *)&theGpio);
  if(status!=XST_SUCCESS)
    return XST_FAILURE;
  // set priority and endge sensitivity
  XScuGic_GetPriorityTriggerType(&interruptController, INTC_AXIGPIO_IRQ_ID, &irqPriority, &irqSensitivity);
  //printf("AXI GPIO old priority=0x%02X; old sensitivity=0x%02X\n\r", irqPriority, irqSensitivity);
  //irqPriority = 0;           // in step of 8; 0=highest; 248=lowest
  irqSensitivity=0x03;         // rising edge
  XScuGic_SetPriorityTriggerType(&interruptController, INTC_AXIGPIO_IRQ_ID, irqPriority, irqSensitivity);
  // enable the IRQ
  XScuGic_Enable(&interruptController, INTC_AXIGPIO_IRQ_ID);
  XGpio_InterruptEnable(&theGpio, GPIO_BUTTON_IRQ_MASK);
  XGpio_InterruptGlobalEnable(&theGpio);


  // Setup AXI timer IRQs ---------------------------------
  // register it as a FIQ; in Vivado prj, the timer IRQ line
  // is physically connected to PS pin "nfiq0_lpd_rpu" (negated)

  // register ISR into standalone AXI timer driver
  //XTmrCtr_SetHandler(&theTimer, (XTmrCtr_Handler)TimerISR, (void *)&theTimer);

  // disable GIC FIQ, so that we get FIQs by dedicated PS pin only
  reg = XScuGic_ReadReg(XPAR_SCUGIC_CPU_BASEADDR, 0);
  XScuGic_WriteReg(XPAR_SCUGIC_CPU_BASEADDR, 0, reg & ~8);

  reg = XScuGic_ReadReg(XPAR_SCUGIC_DIST_BASEADDR, 0);


  // register FIQ interrupt
  Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_FIQ_INT,
    (Xil_ExceptionHandler)FiqHandler,
    (void *)&theTimer);
//  Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_FIQ_INT,
//    (Xil_ExceptionHandler)XTmrCtr_InterruptHandler,
//    (void *)&theTimer);


//  // now register AXI timer driver own ISR into the GIC
//  status=XScuGic_Connect(&interruptController, INTC_TIMER_IRQ_ID,
//                         (Xil_ExceptionHandler)XTmrCtr_InterruptHandler,
//                         (void *)&theTimer);
//  if(status!=XST_SUCCESS)
//    return XST_FAILURE;
//
//  // set priority and endge sensitivity
//  XScuGic_GetPriorityTriggerType(&interruptController, INTC_TIMER_IRQ_ID, &irqPriority, &irqSensitivity);
//  //printf("AXI timer old priority=0x%02X; old sensitivity=0x%02X\n\r", irqPriority, irqSensitivity);
//  irqPriority = 0;           // in step of 8; 0=highest; 248=lowest
//  irqSensitivity=0x03;         // rising edge
//  XScuGic_SetPriorityTriggerType(&interruptController, INTC_TIMER_IRQ_ID, irqPriority, irqSensitivity);
//  // enable the IRQ
//  XScuGic_Enable(&interruptController, INTC_TIMER_IRQ_ID);


  // now enable both IRQ and FIQ
  //Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);
  Xil_ExceptionEnableMask(XIL_EXCEPTION_ALL);

  // now start timer
  XTmrCtr_Start(&theTimer, TIMER_NUMBER);


  return XST_SUCCESS;
  }


// -----------------------------------------------------------

int CleanupIRQs(void)
  {
  // Cleanup PL REGBANK IRQ ---------------------------------
  XScuGic_Disable(&interruptController, INTC_REGBANK_IRQ_ID);
  XScuGic_Disconnect(&interruptController, INTC_REGBANK_IRQ_ID);

  // Cleanup AXI GPIO IRQs ---------------------------------
  XGpio_InterruptGlobalDisable(&theGpio);
  XGpio_InterruptDisable(&theGpio, GPIO_BUTTON_IRQ_MASK);
  XScuGic_Disable(&interruptController, INTC_AXIGPIO_IRQ_ID);
  XScuGic_Disconnect(&interruptController, INTC_AXIGPIO_IRQ_ID);

  // Cleanup AXI timer IRQs ---------------------------------
  XTmrCtr_Stop(&theTimer, TIMER_NUMBER);
  XScuGic_Disable(&interruptController, INTC_TIMER_IRQ_ID);
  XScuGic_Disconnect(&interruptController, INTC_TIMER_IRQ_ID);

  return XST_SUCCESS;
  }


// -----------------------------------------------------------

int SetupAXIGPIO(void)
  {
  int status;

  gpioConfig=XGpio_LookupConfig(GPIO_DEVICE_ID);
  if(NULL==gpioConfig) 
    return XST_FAILURE;

  status=XGpio_CfgInitialize(&theGpio, gpioConfig, gpioConfig->BaseAddress);
  if (status!=XST_SUCCESS)
    return XST_FAILURE;

  status=XGpio_SelfTest(&theGpio);
  if(status!=XST_SUCCESS)
    return XST_FAILURE;

  return XST_SUCCESS;
  }


// -----------------------------------------------------------

int SetupAXItimer(void)
  {
  int status;
  u32 loadreg;

  // XTmrCtr_Initialize calls XTmrCtr_LookupConfig, XTmrCtr_CfgInitialize and XTmrCtr_InitHw
  // I prefer to call them explicitly to get a copy of the configuration structure, 
  // which contains the value of the AXI clock, needed to calculate the timer period.
  gTimerConfig=XTmrCtr_LookupConfig(TIMER_DEVICE_ID);
  if(NULL==gTimerConfig) 
    return XST_FAILURE;
  
  XTmrCtr_CfgInitialize(&theTimer, gTimerConfig, gTimerConfig->BaseAddress);

  status=XTmrCtr_InitHw(&theTimer);
  if(status!=XST_SUCCESS)
    return XST_FAILURE;

  // I don't use XTmrCtr_Initialize; see previous remark
  //status=XTmrCtr_Initialize(&theTimer,TIMER_DEVICE_ID);
  //if(status!=XST_SUCCESS)
  //  return XST_FAILURE;

  status=XTmrCtr_SelfTest(&theTimer, TIMER_NUMBER);
  if(status!=XST_SUCCESS)
    return XST_FAILURE;

  // set timer in generate mode, auto reload, count down (for easier setup of timer period)
  XTmrCtr_SetOptions(&theTimer, TIMER_NUMBER, 
          XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);

  // set timer period
  SetSamplingFreq((u32)gFsampl);
  
  //loadreg=XTmrCtr_ReadReg(theTimer.BaseAddress, TIMER_NUMBER, XTC_TLR_OFFSET);
  //LPRINTF("Timer period= %f s = %u counts\n\r", loadreg/(1.*gTimerConfig->SysClockFreqHz), loadreg);

  gTimerIRQlatency=0;
  gMaxLatency=0;
  gTimerIRQoccurred=0;

  return XST_SUCCESS;
  }


// -----------------------------------------------------------

void SetSamplingFreq(u32 f)
  {
  u32 timerScaler;

  // set timer period
  XTmrCtr_SetResetValue(&theTimer, TIMER_NUMBER, (u32)(gTimerConfig->SysClockFreqHz/f));
  // the actual sampling frequency is truncated to an integer fraction of timer clock (125 MHz);
  // let's retrieve it
  timerScaler=XTmrCtr_ReadReg(theTimer.BaseAddress, TIMER_NUMBER, XTC_TLR_OFFSET);
  gFsampl=gTimerConfig->SysClockFreqHz/(1.0*timerScaler);
  }


// -----------------------------------------------------------

void SetupExceptions(void)
  {
  // ASSERT callback for debug, in case of an exception
  Xil_AssertSetCallback(AssertPrint);
  // register some exception call backs
  Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_DATA_ABORT_INT, LocalAbortHandler, NULL);
  //Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_UNDEFINED_INT, LocalUndefinedHandler, NULL);
  }


// -----------------------------------------------------------

static struct remoteproc *SetupRpmsg(int proc_index, int rsc_index)
  {
  int status;
  void *rsc_table;
  int rsc_size;
  metal_phys_addr_t pa;

  (void)proc_index;    // avoid warning on unused parameter
  rsc_table = get_resource_table(rsc_index, &rsc_size);
  // register IPI device
  if(metal_register_generic_device(&ipi_device))
    return NULL;
  // init remoteproc instance
  if(!remoteproc_init(&rproc_inst, &rproc_ops, &rproc_priv))
    return NULL;
  // mmap resource table
  pa = (metal_phys_addr_t)rsc_table;
  (void *)remoteproc_mmap(&rproc_inst, &pa,
    NULL, rsc_size,
    NORM_NSHARED_NCACHE|PRIV_RW_USER_RW,
    &rproc_inst.rsc_io);
  // mmap shared memory
  pa = SHARED_MEM_PA;
  (void *)remoteproc_mmap(&rproc_inst, &pa,
    NULL, SHARED_MEM_SIZE,
    NORM_NSHARED_NCACHE|PRIV_RW_USER_RW,
    NULL);
  // parse resource table to remoteproc
  status = remoteproc_set_rsc_table(&rproc_inst, rsc_table, rsc_size);
  if(status!=0) 
    {
    LPRINTF("Failed to initialize remoteproc\n\r");
    remoteproc_remove(&rproc_inst);
    return NULL;
    }
  LPRINTF("Remoteproc successfully initialized\n\r");

  return &rproc_inst;
  }


// -----------------------------------------------------------

int SetupSystem(void **platformp)
  {
  int status;
  u8 rdbck;
  struct remoteproc *rproc;
  struct metal_init_params metal_param = METAL_INIT_DEFAULTS;
  unsigned int irqflags;

  metal_param.log_level = METAL_LOG_INFO;

  if(!platformp)
    {
    LPRINTF("NULL platform pointer -");
    return -EINVAL;
    }
  
  metal_init(&metal_param);

  SetupExceptions();

  // setup PL stuff
  status = SetupAXIGPIO();
  if(status!=XST_SUCCESS)
    return XST_FAILURE;

  status = SetupAXItimer();
  if(status!=XST_SUCCESS)
    return XST_FAILURE;

  // IRQ
  status = SetupIRQs();
  if(status!=XST_SUCCESS)
    return XST_FAILURE;

  // setup OpenAMP
  rproc = SetupRpmsg(0,0);
  if(!rproc)
    return XST_FAILURE;

  *platformp = rproc;

  rpdev = create_rpmsg_vdev(rproc, 0, VIRTIO_DEV_DEVICE, NULL, NULL);
  if(!rpdev)
    {
    LPERROR("Failed to create rpmsg virtio device\n\r");
    return XST_FAILURE;
    }

  // init RPMSG framework
  status = rpmsg_create_ept(&lept, rpdev, RPMSG_SERVICE_NAME,
                            RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
                            rpmsg_endpoint_cb,
                            rpmsg_service_unbind);
  if(status)
    {
    LPERROR("Failed to create endpoint\n\r");
    return XST_FAILURE;
    }
  LPRINTF("created rpmsg endpoint\n\r");

  // init shared memory for ADC samples
  status = InitSampleShmem();
  if(status!=XST_SUCCESS)
    {
    LPRINTF("Error in ADC sample shared memory initialization.\r\n");
    return XST_FAILURE;
    }
  else
    {
    LPRINTF("ADC sample shared memory successfully initialized\r\n");
    }

  // setup analog card (ADI CN0585)
  status = InitMAX7301();
  if(status!=XST_SUCCESS)
    {
    LPRINTF("Error in MAX7301 initialization.\r\n");
    return XST_FAILURE;
    }
  else
    {
    LPRINTF("MAX7301 successfully initialized\r\n");
    }

  status = InitAD3552();
  if(status!=XST_SUCCESS)
    {
    LPRINTF("Error in AD3552 initialization.\r\n");
    return XST_FAILURE;
    }
  else
    {
    LPRINTF("AD3552 successfully initialized\r\n");
    }

  status = InitADAQ23876();
  if(status!=XST_SUCCESS)
    {
    LPRINTF("Error in ADAQ23876 initialization.\r\n");
    return XST_FAILURE;
    }
  else
    {
    LPRINTF("ADAQ23876 successfully initialized\r\n");
    }

  // set a known initial pattern on the DAC outputs for debug purposes
  status = WriteDacSamples(0,0x4000, 0xC000);
  status = UpdateDacOutput(0);
  status = WriteDacSamples(1,0xC000, 0x4000);
  status = UpdateDacOutput(1);

  return XST_SUCCESS;
  }

// -----------------------------------------------------------

int CleanupSystem(void *platform)
  {
  int status;
  struct remoteproc *rproc = platform;
  
  status = CleanupIRQs();

  rpmsg_destroy_ept(&lept);
  release_rpmsg_vdev(rpdev, platform);

  if(rproc)
    remoteproc_remove(rproc);

  ReleaseSampleShmem();
  
  metal_finish();

  Xil_DCacheDisable();
  Xil_ICacheDisable();
  Xil_DCacheInvalidate();
  Xil_ICacheInvalidate();

  return status;
  }


// -----------------------------------------------------------

void ResetTimeTable(void)
  {
  int i,j;
  for(i=0; i<PROFILE_TIME_ENTRIES; i++)
    for(j=0; j<5; j++)
      time_table[i][j]=0.0;
  }

// -----------------------------------------------------------

void AddTimeToTable(int theindex, double thetime)
  {
  // add time value to table for profiling
  // entries are <x>, <x^2>, min, max, N_entries
  double N;
  
  // min
  if(thetime<time_table[theindex][PROFTIME_MIN])
    time_table[theindex][PROFTIME_MIN]=thetime;

  // max
  if(thetime>time_table[theindex][PROFTIME_MAX])
    time_table[theindex][PROFTIME_MAX]=thetime;
  
  // increment num of entries for this time
  N= ++time_table[theindex][PROFTIME_N];

  // x mean
  time_table[theindex][PROFTIME_AVG] = time_table[theindex][PROFTIME_AVG] *(N-1)/N + thetime/N;

  // x^2 mean
  time_table[theindex][PROFTIME_AVG2] = time_table[theindex][PROFTIME_AVG2] *(N-1)/N + thetime*thetime/N;
  }


// -----------------------------------------------------------

void InitVars(void)
  {
  int i;

  // init vars
  gFsampl = DEFAULT_TIMER_FREQ_HZ;
  g2pi=8.*atan(1.);
  gR5ctrlState=R5CTRLR_IDLE;
  for(i=0; i<4; i++)
    {
    g_y[i]=0.;
    gPhase[i]=0.;
    gFreq[i]=0.;
    gTotSweepSamples[i]=0;
    gCurSweepSamples[i]=0;
    gWavegenChanConfig[i].enable = WGEN_CH_ENABLE_OFF;
    gWavegenChanConfig[i].type   = WGEN_CH_TYPE_DC;
    gWavegenChanConfig[i].ampl   = 0.;
    gWavegenChanConfig[i].offs   = 0.;
    gWavegenChanConfig[i].f1     = 0.;
    gWavegenChanConfig[i].f2     = 0.;
    gWavegenChanConfig[i].dt     = 1.;
    }

  // init number of IRQ served
  last_irq_cnt=0;
  for(i=0; i<4; i++)
    irq_cntr[i]=0;
  }

// -----------------------------------------------------------

int main()
  {
  unsigned int thereg, theval;
  int          status, i;
  double       currtimer_us, sigma;
  double       dphase, alpha, dfreq;

  // remove buffering from stdin and stdout
  setvbuf (stdout, NULL, _IONBF, 0);
  setvbuf (stdin, NULL, _IONBF, 0);
  
  InitVars();

  // init profiling time table
  ResetTimeTable();

  LPRINTF("\n\r*** R5 integrated controller ***\n\r\n\r");
  LPRINTF("This is R5/baremetal side\n\r\n\r");

  LPRINTF("openamp lib version: %s (", openamp_version());
  LPRINTF("Major: %d, ", openamp_version_major());
  LPRINTF("Minor: %d, ", openamp_version_minor());
  LPRINTF("Patch: %d)\n\r", openamp_version_patch());

  LPRINTF("libmetal lib version: %s (", metal_ver());
  LPRINTF("Major: %d, ", metal_ver_major());
  LPRINTF("Minor: %d, ", metal_ver_minor());
  LPRINTF("Patch: %d)\n\r", metal_ver_patch());

  status = SetupSystem(&gplatform);
  if(status!=XST_SUCCESS)
    {
    LPRINTF("ERROR Setting up System - aborting\n\r");
    return status;
    }

  
  shutdown_req = 0;  
  // shutdown_req will be set set to 1 by RPMSG unbind callback
  while(!shutdown_req)
    {
    // remember you can't use sscanf in the main loop, to avoid loosing RPMSGs

    _rproc_wait();
    // check whether we have a message from A53/linux
    (void)remoteproc_get_notification(gplatform, RSC_NOTIFY_ID_ANY);

    if(gTimerIRQoccurred!=0)
      {
      // reset timer IRQ flag
      gTimerIRQoccurred=0;

      // register IRQ latency as row 0 of prifile time table
      #ifdef PROFILE
      AddTimeToTable(0,gTimerIRQlatency);
      #endif

      // register start time of control loop step calculation
      #ifdef PROFILE
      currtimer_us=GetTimer_us();
      AddTimeToTable(1,currtimer_us);
      #endif

      // do something...

      // read ADCs into raw (adcval[i]) and with fullscale = 1.0 (g_x[i])
      ReadADCs(adcval);
      for(i=0; i<4; i++)
        g_x[i]=adcval[i]/ADAQ23876_FULLSCALE_CNT;

      switch(gR5ctrlState)
        {
        case R5CTRLR_IDLE:
          break;
        
        case R5CTRLR_WAVEGEN:
          for(i=0; i<4; i++)
            {
            if(gWavegenChanConfig[i].enable==WGEN_CH_ENABLE_OFF)
              g_y[i]=0.0;
            else
              {
              //channel is enabled
              switch(gWavegenChanConfig[i].type)
                {
                case WGEN_CH_TYPE_DC:
                  g_y[i]=gWavegenChanConfig[i].ampl;
                  break;

                case WGEN_CH_TYPE_SINE:
                  // output current phase
                  g_y[i]=sin(gPhase[i])*gWavegenChanConfig[i].ampl + gWavegenChanConfig[i].offs;
                  // calculate next phase
                  dphase = g2pi*gWavegenChanConfig[i].f1/gFsampl;
                  gPhase[i] += dphase;
                  if(gPhase[i]>g2pi)
                    gPhase[i] -= g2pi;
                  break;

                case WGEN_CH_TYPE_SWEEP:
                  // output current phase
                  g_y[i]=sin(gPhase[i])*gWavegenChanConfig[i].ampl + gWavegenChanConfig[i].offs;
                  // calculate next phase
                  if(gWavegenChanConfig[i].dt<__DBL_EPSILON__)
                    {
                    alpha=0;
                    gTotSweepSamples[i]=0;
                    }
                  else 
                    {
                    alpha=((double)(gWavegenChanConfig[i].f2)-gWavegenChanConfig[i].f1)/gWavegenChanConfig[i].dt;
                    gTotSweepSamples[i]=gWavegenChanConfig[i].dt*gFsampl;
                    }

                  gFreq[i]  += alpha/gFsampl;
                  dphase     = g2pi*((double)(gWavegenChanConfig[i].f1)+gFreq[i])/gFsampl;
                  gPhase[i] += dphase;
                  if(gPhase[i]>g2pi)
                    gPhase[i] -= g2pi;
                  
                  // check sweep status
                  gCurSweepSamples[i]++;
                  if(gCurSweepSamples[i]>=gTotSweepSamples[i])
                    {
                    // end of sweep
                    if(gWavegenChanConfig[i].enable==WGEN_CH_ENABLE_SINGLE)
                      {
                      // stop after a single sweep
                      gWavegenChanConfig[i].enable=WGEN_CH_ENABLE_OFF;
                      gPhase[i]=0.;
                      gFreq[i]=0.;
                      }
                    else
                      {
                      // sweep forever: restart from f1
                      gPhase[i]=0.;
                      gFreq[i]=0.;
                      gCurSweepSamples[i]=0;                      
                      }
                    }
                  break;
                }
              }
            }
          break;
        }
      

      // register time of end of sine wave calculation
      #ifdef PROFILE
      currtimer_us=GetTimer_us();
      AddTimeToTable(2,currtimer_us);
      #endif

      // write DACs from values with fullscale = 1 (g_y[i]) into raw (dacval[i])

      // DAC AD3552 is offset binary;
      // usual conversion from 2's complement is done inverting the MSB,
      // but here I prefer to keep a fullscale of +/-1 (double) in the calculations,
      // so I just scale and offset at the end, which is clearer
      for(i=0; i<4; i++)
        dacval[i]=(u16)round(g_y[i]*AD3552_AMPL+AD3552_OFFS);
      
      status = WriteDacSamples(0,dacval[0], dacval[1]);
      status = WriteDacSamples(1,dacval[2], dacval[3]);
      // DAC output will be updated by hardware /LDAC pulse on next cycle 
      //status = UpdateDacOutput(0);
      //status = UpdateDacOutput(1);

      // register time of end of control loop step
      #ifdef PROFILE
      currtimer_us=GetTimer_us();
      AddTimeToTable(PROFILE_TIME_ENTRIES-1,currtimer_us);
      #endif

      // write something in ADCsample shared memory
      // for debug I write the number of timer IRQs at offset 0
      // metal_io_write32(sample_shmem_io, 0, irq_cntr[TIMER_IRQ_CNTR]);

      #ifdef PRINTOUT
      // every now and then print something
      if(irq_cntr[TIMER_IRQ_CNTR]-last_irq_cnt >= gFsampl)
        {
        last_irq_cnt = irq_cntr[TIMER_IRQ_CNTR];

        // // print current state
        // LPRINTF("R5 state is ");
        // switch(gR5ctrlState)
        //   {
        //   case R5CTRLR_IDLE:
        //     LPRINTF("IDLE\n\r");
        //     break;
        //   case R5CTRLR_WAVEGEN:
        //     LPRINTF("WAVEGEN\n\r");
        //     break;
        //   }
          
        //  // print IRQ number
        //  LPRINTF("Tot IRQs so far: %d \n\r",last_irq_cnt);

        // in IDLE mode print ADC values
        if( gR5ctrlState == R5CTRLR_IDLE )
          {
          for(i=0; i<4; i++)
            // LPRINTF("ADC#%d = %3d.%03d ",
            //         i,
            //         (int)(g_x[i]*ADAQ23876_FULLSCALE_VOLT),
            //         DECIMALS(g_x[i]*ADAQ23876_FULLSCALE_VOLT,3)
            //        );
            LPRINTF("ADC#%d = %6d ",i, adcval[i]);
          LPRINTF("\n\r");
          }

        // in WAVEGEN mode print channel configurations
        if( gR5ctrlState == R5CTRLR_WAVEGEN )
          {
          for(i=0; i<4; i++)
            {
            LPRINTF("WAVEGEN CH# %d ",i+1);

            switch(gWavegenChanConfig[i].enable)
              {
              case WGEN_CH_ENABLE_OFF:
                LPRINTF("   OFF  ");
                break;
              case WGEN_CH_ENABLE_ON:
                LPRINTF("   ON   ");
                break;
              case WGEN_CH_ENABLE_SINGLE:
                LPRINTF(" SINGLE ");
                break;
              }

            switch(gWavegenChanConfig[i].type)
              {
              case WGEN_CH_TYPE_DC:
                LPRINTF("   DC  ");
                break;
              case WGEN_CH_TYPE_SINE:
                LPRINTF("  SINE ");
                break;
              case WGEN_CH_TYPE_SWEEP:
                LPRINTF(" SWEEP ");
                break;
              }
            
            // R5 cannot print floats with xil_printf,
            // so we use a few macros; 
            // note that "SIGN" is needed to properly print 
            // values in range (0,-1)

            // amplitude is positive
            LPRINTF(" A= %d.%03d ",
                    (int)(fabs(gWavegenChanConfig[i].ampl)),
                    DECIMALS(gWavegenChanConfig[i].ampl,3)
                   );
            // offset may be negative
            LPRINTF(" offs= %c%d.%03d ",
                    SIGN(gWavegenChanConfig[i].offs),
                    (int)(gWavegenChanConfig[i].offs),
                    DECIMALS(gWavegenChanConfig[i].offs,3)
                   );
            // fstart is positive
            LPRINTF(" f1= %3d.%03d ",
                    (int)(gWavegenChanConfig[i].f1),
                    DECIMALS(gWavegenChanConfig[i].f1,3)
                   );
            // fstop is positive
            LPRINTF(" f2= %3d.%03d ",
                    (int)(gWavegenChanConfig[i].f2),
                    DECIMALS(gWavegenChanConfig[i].f2,3)
                   );
            // sweep time is positive
            LPRINTF(" dt= %3d.%03d ",
                    (int)(gWavegenChanConfig[i].dt),
                    DECIMALS(gWavegenChanConfig[i].dt,3)
                   );

            // // print alpha for debug
            // if(gWavegenChanConfig[i].dt<__DBL_EPSILON__)
            //   alpha=0;
            // else 
            //   alpha=((double)(gWavegenChanConfig[i].f2)-gWavegenChanConfig[i].f1)/gWavegenChanConfig[i].dt;
            // LPRINTF(" alpha= %3d.%010d ",
            //         (int)(alpha),
            //         DECIMALS(alpha,10)
            //        );

            LPRINTF("\n\r");
            }
          LPRINTF("\n\r");
          }

        // print profiling info
        #ifdef PROFILE
        LPRINTF("Number of Timer IRQs          : %d\n\r", last_irq_cnt);

        LPRINTF("Timer IRQ latency (us)        : avg= %d.%03d ; max= %d.%03d\n\r", 
          (int)(time_table[0][PROFTIME_AVG]), DECIMALS(time_table[0][PROFTIME_AVG],3),
          (int)(time_table[0][PROFTIME_MAX]), DECIMALS(time_table[0][PROFTIME_MAX],3) );

        LPRINTF("Ctrl loop step start (us)     : avg= %d.%03d ; max= %d.%03d\n\r", 
          (int)(time_table[1][PROFTIME_AVG]), DECIMALS(time_table[1][PROFTIME_AVG],3),
          (int)(time_table[1][PROFTIME_MAX]), DECIMALS(time_table[1][PROFTIME_MAX],3) );
  
        sigma=time_table[2][PROFTIME_AVG2]-time_table[2][PROFTIME_AVG]*time_table[2][PROFTIME_AVG];
        LPRINTF("End of Sine Wave Calc (us)    : avg= %d ; s= %d.%03d; max= %d\n\r", 
          (int)(time_table[2][PROFTIME_AVG]), 
          (int)(sigma), DECIMALS(sigma, 3),
          (int)(time_table[2][PROFTIME_MAX]) );

        LPRINTF("Ctrl loop step end (us)       : avg= %d ; max= %d\n\r", 
          (int)(time_table[PROFILE_TIME_ENTRIES-1][PROFTIME_AVG]), 
          (int)(time_table[PROFILE_TIME_ENTRIES-1][PROFTIME_MAX]) );
  
        LPRINTF("\n\r");
        #endif  // PROFILE
        }
      #endif  // PRINTOUT

      }  // if timer occurred

    }  // if not shutdown

  LPRINTF("\n\rExiting\n\r");

  status = CleanupSystem(gplatform);
  if(status!=XST_SUCCESS)
    {
    LPRINTF("ERROR Cleaning up System\n\r");
    // continue anyway, as we are shutting down
    //return status;
    }

  return 0;
  }
