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
//
// latest rev: mar 6 2025
//

#include "r5_main.h"


// ##########  globals  #######################

void *platform;
// openamp
static struct rpmsg_endpoint lept;
static int shutdown_req = 0;
static LOOP_PARAM_MSG_TYPE gLoopParameters;
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
static XTmrCtr_Config *timerConfig;
float gTimerIRQlatency, gMaxLatency;
int gTimerIRQoccurred;

u16    dacval[4];
s16    adcval[4];
double g2pi, gPhase, gdPhase, gFreq, gAmpl, gDCval, g_x[4], g_y[4];
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

  // update the total number of received messages, for debug purposes
  irq_cntr[IPI_CNTR]++;

  // use msg to update parameters
  if(len<sizeof(LOOP_PARAM_MSG_TYPE))
    {
    LPRINTF("incomplete message received.\n\r");
    return RPMSG_ERR_BUFF_SIZE;
    }
  memcpy(&gLoopParameters, data, sizeof(LOOP_PARAM_MSG_TYPE));
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
  return 1.e6*(loadReg - cnts)/(1.0*timerConfig->SysClockFreqHz);
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
  gTimerIRQlatency=(timerConfig->SysClockFreqHz/TIMER_FREQ_HZ - cnts*1.0)/timerConfig->SysClockFreqHz;

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
  timerConfig=XTmrCtr_LookupConfig(TIMER_DEVICE_ID);
  if(NULL==timerConfig) 
    return XST_FAILURE;
  
  XTmrCtr_CfgInitialize(&theTimer, timerConfig, timerConfig->BaseAddress);

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
  XTmrCtr_SetResetValue(&theTimer, TIMER_NUMBER,
          (u32)(timerConfig->SysClockFreqHz/TIMER_FREQ_HZ));
  
  //loadreg=XTmrCtr_ReadReg(theTimer.BaseAddress, TIMER_NUMBER, XTC_TLR_OFFSET);
  //LPRINTF("Timer period= %f s = %u counts\n\r", loadreg/(1.*timerConfig->SysClockFreqHz), loadreg);

  gTimerIRQlatency=0;
  gMaxLatency=0;
  gTimerIRQoccurred=0;

  return XST_SUCCESS;
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

int main()
  {
  unsigned int thereg, theval;
  int          status, i;
  double       currtimer_us, sigma;

  // remove buffering from stdin and stdout
  setvbuf (stdout, NULL, _IONBF, 0);
  setvbuf (stdin, NULL, _IONBF, 0);
  
  // init number of IRQ served
  last_irq_cnt=0;
  for(i=0; i<4; i++)
    irq_cntr[i]=0;

  // init sample loop parameters
  gLoopParameters.freqHz=1000.f;
  gLoopParameters.percentAmplitude=75;
  gLoopParameters.constValVolt=3.5f;

  // init profiling time table
  ResetTimeTable();

  LPRINTF("\n\r*** R5 integrated controller ***\n\r\n\r");
#ifdef ARMR5
  LPRINTF("This is R5/baremetal side\n\r\n\r");
#else
  LPRINTF("This is A53/linux side\n\r\n\r");
#endif

  LPRINTF("openamp lib version: %s (", openamp_version());
  LPRINTF("Major: %d, ", openamp_version_major());
  LPRINTF("Minor: %d, ", openamp_version_minor());
  LPRINTF("Patch: %d)\n\r", openamp_version_patch());

  LPRINTF("libmetal lib version: %s (", metal_ver());
  LPRINTF("Major: %d, ", metal_ver_major());
  LPRINTF("Minor: %d, ", metal_ver_minor());
  LPRINTF("Patch: %d)\n\r", metal_ver_patch());

  status = SetupSystem(&platform);
  if(status!=XST_SUCCESS)
    {
    LPRINTF("ERROR Setting up System - aborting\n\r");
    return status;
    }

  g2pi=8.*atan(1.);
  gPhase=0.0;
  
  shutdown_req = 0;  
  // shutdown_req will be set set to 1 by RPMSG unbind callback
  while(!shutdown_req)
    {
    // remember you can't use sscanf in the main loop, to avoid loosing RPMSGs

    // LPRINTF("\n\rTimer   IRQs            : %d\n\r",irq_cntr[TIMER_IRQ_CNTR]);
    // LPRINTF(    "GPIO    IRQs            : %d\n\r",irq_cntr[GPIO_IRQ_CNTR]);
    // LPRINTF(    "RegBank IRQs            : %d\n\r",irq_cntr[REGBANK_IRQ_CNTR]);
    // LPRINTF(    "RPMSG   IPIs            : %d\n\r",irq_cntr[IPI_CNTR]);
    // LPRINTF(    "Loop Parameter 1 (float): %d.%03d \n\r",
    //     (int)(gLoopParameters.param1),
    //     DECIMALS(gLoopParameters.param1,3));
    // LPRINTF(    "Loop Parameter 2 (int)  : %d\n\r",gLoopParameters.param2);
    // LPRINTF(    "Timer IRQ latency (ns)  : current= %d ; max= %d\n\r", (int)(gTimerIRQlatency*1.e9), (int)(gMaxLatency*1.e9));
    // // can't use sscanf in the main loop, to avoid loosing RPMSGs,
    // // so I print all the registers each time I get an IRQ,
    // // which is at least once a second from the timer IRQ
    // for(thereg=0; thereg<16; thereg++)
    //   LPRINTF(  "Regbank[%02d]             : 0x%08X\n\r",(int)(thereg), *(REGBANK+thereg));

    _rproc_wait();
    // check whether we have a message from A53/linux
    (void)remoteproc_get_notification(platform, RSC_NOTIFY_ID_ANY);

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
      
      // read ADCs with fullscale = 1.0
      ReadADCs(adcval);
      for(i=0; i<4; i++)
        g_x[i]=adcval[i]/ADAQ23876_FULLSCALE_CNT;

      // workout next DAC value
      gFreq=gLoopParameters.freqHz;
      gAmpl=gLoopParameters.percentAmplitude*0.01;
      gDCval=gLoopParameters.constValVolt;
      gdPhase= g2pi*gFreq/(double)(TIMER_FREQ_HZ);
      gPhase += gdPhase;
      if(gPhase>g2pi)
        gPhase -= g2pi;
      // work out next DAC values with fullscale = 1.0
      g_y[0]=gAmpl*sin(gPhase);
      g_y[1]=gAmpl*cos(gPhase);
      g_y[2]=gAmpl*sin(2.*gPhase);
      g_y[3]=gDCval/AD3552_FULLSCALE_VOLT;

      // DAC AD3552 is offset binary;
      // usual conversion from 2's complement is done inverting the MSB,
      // but here I prefer to keep a fullscale of +/-1 (double) in the calculations,
      // so I just scale and offset at the end, which is clearer
      for(i=0; i<4; i++)
        dacval[i]=(u16)round(g_y[i]*AD3552_AMPL+AD3552_OFFS);

      // register time of end of sine wave calculation
      #ifdef PROFILE
      currtimer_us=GetTimer_us();
      AddTimeToTable(2,currtimer_us);
      #endif

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
      metal_io_write32(sample_shmem_io, 0, irq_cntr[TIMER_IRQ_CNTR]);

      // every now and then print something
      if(irq_cntr[TIMER_IRQ_CNTR]-last_irq_cnt >= TIMER_FREQ_HZ)
        {
        last_irq_cnt = irq_cntr[TIMER_IRQ_CNTR];

        // print ADC values in volt
        for(i=0; i<4; i++)
          // LPRINTF("ADC#%d = %3d.%03d ",
          //         i,
          //         (int)(g_x[i]*ADAQ23876_FULLSCALE_VOLT),
          //         DECIMALS(g_x[i]*ADAQ23876_FULLSCALE_VOLT,3)
          //        );
          LPRINTF("ADC#%d = %6d ",i, adcval[i]);
        LPRINTF("\n\r");

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
        #endif
        }

      }  // if timer occurred

    }  // if not shutdown

  LPRINTF("\n\rExiting\n\r");

  status = CleanupSystem(platform);
  if(status!=XST_SUCCESS)
    {
    LPRINTF("ERROR Cleaning up System\n\r");
    // continue anyway, as we are shutting down
    //return status;
    }

  return 0;
  }
