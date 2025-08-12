//
// R5 integrated controller 
// ZCU102 + ADI CN0585/CN0584
//
// IRQs from PL and 
// interproc communications with A53/linux
//
//////////////////////////////////////////
//
// this is the linux/A53 side
//

#include "linux_main.h"

// ##########  globals  #######################

void *gplatform;
// openamp
RPMSG_ENDP_TYPE endp;
R5_RPMSG_TYPE *gMsgPtr;
struct rpmsg_device *rpdev;
static struct remoteproc rproc_inst;
static int ept_deleted = 0;
int gIncomingRpmsgs;

s16 g_adcval[4], g_dacval[4];
s16 gADC_offs_cnt[4], gDAC_offs_cnt[4];
int gDAC_outputSelect[4];
float gADC_gain[4];
u32 gFsampl;     // R5 sampling frequency rounded to 1 Hz precision
int gR5ctrlState;
WAVEGEN_CH_CONFIG gWavegenChanConfig[4];
TRIG_CONFIG gRecorderConfig;
CTRLLOOP_CH_CONFIG gCtrlLoopChanConfig[4];


struct remoteproc_priv rproc_priv = 
  {
  .shm_name = SHM_DEV_NAME,
  .shm_bus_name = DEV_BUS_NAME,
  .ipi_name = IPI_DEV_NAME,
  .ipi_bus_name = DEV_BUS_NAME,
  .ipi_chn_mask = IPI_CHN_BITMASK,
  };


// ##########  implementation  ################

void FlushRpmsg(void)
  {
  // remove stale rpmsg in queue
  do
    {
    gIncomingRpmsgs=0;
    (void)remoteproc_get_notification(gplatform, RSC_NOTIFY_ID_ANY);
    // the call to the callback increments gIncomingRpmsgs in case a msg was present
    } while(gIncomingRpmsgs>0);
  }


// -----------------------------------------------------------

int WaitForRpmsg(void)
  {
  int ret;
  time_t startt, currt;
  double dt;

  time(&startt);
  do
    {
    // release CPU and put this thread to the bottom of the scheduler list
    metal_cpu_yield();
    ret=remoteproc_get_notification(gplatform, RSC_NOTIFY_ID_ANY);
    time(&currt);
    dt=difftime(currt, startt);
    } while((gIncomingRpmsgs==0) && (ret==0) && (dt<RPMSG_ANSWER_TIMEOUT_SEC));

  if(dt>=RPMSG_ANSWER_TIMEOUT_SEC)
    return RPMSG_ANSWER_TIMEOUT;
  else if(ret!=0)
    return RPMSG_ANSWER_ERR;
  else
    return RPMSG_ANSWER_VALID;
  }


// -----------------------------------------------------------

// callback for messages from R5
static int rpmsg_endpoint_cb(struct rpmsg_endpoint *ept, void *data, size_t len, uint32_t src, void *priv)
  {
  (void)ept;    // avoid warning on unused parameter
  (void)data;   // avoid warning on unused parameter
  (void)len;    // avoid warning on unused parameter
  (void)src;    // avoid warning on unused parameter
  (void)priv;   // avoid warning on unused parameter

  u32 cmd, d;
  int nch, i;
  float *xp;

  gIncomingRpmsgs++;

  if(len<sizeof(R5_RPMSG_TYPE))
    {
    LPRINTF("incomplete message received.\n\r");
    return RPMSG_ERR_BUFF_SIZE;
    }

  cmd=((R5_RPMSG_TYPE*)data)->command;

  switch(cmd)
    {
    // readback DAC values sent by R5
    case RPMSGCMD_READ_DAC:
      d=((R5_RPMSG_TYPE*)data)->data[0];
      g_dacval[0]=(s16)(d&0x0000FFFF);
      g_dacval[1]=(s16)((d>>16)&0x0000FFFF);
      d=((R5_RPMSG_TYPE*)data)->data[1];
      g_dacval[2]=(s16)(d&0x0000FFFF);
      g_dacval[3]=(s16)((d>>16)&0x0000FFFF);
      break;

    // readback DAC offset sent by R5
    case RPMSGCMD_READ_DACOFFS:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      gDAC_offs_cnt[nch-1] = (s16)(((R5_RPMSG_TYPE*)data)->data[1]);
      break;

    // readback DAC output selection
    case RPMSGCMD_READ_DACOUTSEL:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      
      gDAC_outputSelect[nch-1] = (int)(((R5_RPMSG_TYPE*)data)->data[1])-1;
      break;

    // readback ADC values sent by R5
    case RPMSGCMD_READ_ADC:
      d=((R5_RPMSG_TYPE*)data)->data[0];
      g_adcval[0]=(s16)(d&0x0000FFFF);
      g_adcval[1]=(s16)((d>>16)&0x0000FFFF);
      d=((R5_RPMSG_TYPE*)data)->data[1];
      g_adcval[2]=(s16)(d&0x0000FFFF);
      g_adcval[3]=(s16)((d>>16)&0x0000FFFF);
      break;

    // readback ADC offset sent by R5
    case RPMSGCMD_READ_ADCOFFS:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      gADC_offs_cnt[nch-1] = (s16)(((R5_RPMSG_TYPE*)data)->data[1]);
      break;

    // readback ADC gain sent by R5
    case RPMSGCMD_READ_ADCGAIN:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      // read floating point values directly as float (32 bit)
      memcpy(&(gADC_gain[nch-1]), &(((R5_RPMSG_TYPE*)data)->data[1]), sizeof(u32));
      break;

    // readback sampling frequency
    case RPMSGCMD_READ_FSAMPL:
      gFsampl=((R5_RPMSG_TYPE*)data)->data[0];
      break;

    // readback state
    case RPMSGCMD_READ_STATE:
      gR5ctrlState=(int)((R5_RPMSG_TYPE*)data)->data[0];
      break;

    // readback waveform generator channel config
    case RPMSGCMD_READ_WGEN_CH_CONF:
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
      break;

    // readback waveform generator channel on/off state
    case RPMSGCMD_READ_WGEN_CH_STATE:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      
      gWavegenChanConfig[nch-1].state   = (int)(((R5_RPMSG_TYPE*)data)->data[1]);
      break;

    // readback control loop channel on/off state
    case RPMSGCMD_READ_CTRLLOOP_CH_STATE:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      
      gCtrlLoopChanConfig[nch-1].state   = (int)(((R5_RPMSG_TYPE*)data)->data[1]);
      break;

    // readback control loop channel input selector
    case RPMSGCMD_READ_CTRLLOOP_CH_INSEL:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      d=(int)(((R5_RPMSG_TYPE*)data)->data[1]);
      if((d<1)||(d>5))
        return RPMSG_ERR_PARAM;
      
      gCtrlLoopChanConfig[nch-1].inputSelect=d-1;
      break;

    // readback PID gains
    case RPMSGCMD_READ_PID_GAINS:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      d=(int)(((R5_RPMSG_TYPE*)data)->data[1]);
      if((d<1)||(d>2))
        return RPMSG_ERR_PARAM;
      
      // read floating point values directly as float (32 bit)
      memcpy(&(gCtrlLoopChanConfig[nch-1].PID[d-1].Gp),    &(((R5_RPMSG_TYPE*)data)->data[2]), sizeof(u32));
      memcpy(&(gCtrlLoopChanConfig[nch-1].PID[d-1].Gi),    &(((R5_RPMSG_TYPE*)data)->data[3]), sizeof(u32));
      memcpy(&(gCtrlLoopChanConfig[nch-1].PID[d-1].G1d),   &(((R5_RPMSG_TYPE*)data)->data[4]), sizeof(u32));
      memcpy(&(gCtrlLoopChanConfig[nch-1].PID[d-1].G2d),   &(((R5_RPMSG_TYPE*)data)->data[5]), sizeof(u32));
      memcpy(&(gCtrlLoopChanConfig[nch-1].PID[d-1].G_aiw), &(((R5_RPMSG_TYPE*)data)->data[6]), sizeof(u32));
      break;


    // readback IIR coeffs
    case RPMSGCMD_READ_IIR_COEFF:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      d=(int)(((R5_RPMSG_TYPE*)data)->data[1]);
      if((d<1)||(d>2))
        return RPMSG_ERR_PARAM;
      
      // read floating point values directly as float (32 bit)
      memcpy(&(gCtrlLoopChanConfig[nch-1].IIR[d-1].a[0]), &(((R5_RPMSG_TYPE*)data)->data[2]), sizeof(u32));
      memcpy(&(gCtrlLoopChanConfig[nch-1].IIR[d-1].a[1]), &(((R5_RPMSG_TYPE*)data)->data[3]), sizeof(u32));
      memcpy(&(gCtrlLoopChanConfig[nch-1].IIR[d-1].a[2]), &(((R5_RPMSG_TYPE*)data)->data[4]), sizeof(u32));
      memcpy(&(gCtrlLoopChanConfig[nch-1].IIR[d-1].b[0]), &(((R5_RPMSG_TYPE*)data)->data[5]), sizeof(u32));
      memcpy(&(gCtrlLoopChanConfig[nch-1].IIR[d-1].b[1]), &(((R5_RPMSG_TYPE*)data)->data[6]), sizeof(u32));
      break;


    // readback MIMO matrix row
    case RPMSGCMD_READ_MATRIX_ROW:
      d=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((d<0)||(d>5))
        return RPMSG_ERR_PARAM;
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[1]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;

      switch(d)
          {
          case 0:
            xp=gCtrlLoopChanConfig[nch-1].input_MISO_A;
            break;
          case 1:
            xp=gCtrlLoopChanConfig[nch-1].input_MISO_B;
            break;
          case 2:
            xp=gCtrlLoopChanConfig[nch-1].input_MISO_C;
            break;
          case 3:
            xp=gCtrlLoopChanConfig[nch-1].input_MISO_D;
            break;
          case 4:
            xp=gCtrlLoopChanConfig[nch-1].output_MISO_E;
            break;
          case 5:
            xp=gCtrlLoopChanConfig[nch-1].output_MISO_F;
            break;
          }

      // read floating point values directly as float (32 bit)
      for(i=0; i<5; i++)
        memcpy(xp+i, &(((R5_RPMSG_TYPE*)data)->data[2+i]), sizeof(u32));
      
      break;


    // readback PID thresholds
    case RPMSGCMD_READ_PID_THR:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      d=(int)(((R5_RPMSG_TYPE*)data)->data[1]);
      if((d<1)||(d>2))
        return RPMSG_ERR_PARAM;
      
      // read floating point values directly as float (32 bit)
      memcpy(&(gCtrlLoopChanConfig[nch-1].PID[d-1].in_thr),    &(((R5_RPMSG_TYPE*)data)->data[2]), sizeof(u32));
      memcpy(&(gCtrlLoopChanConfig[nch-1].PID[d-1].out_sat),   &(((R5_RPMSG_TYPE*)data)->data[3]), sizeof(u32));
      break;

    // readback PID derivative on process variable
    case RPMSGCMD_READ_PID_PV_DERIV:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      d=(int)(((R5_RPMSG_TYPE*)data)->data[1]);
      if((d<1)||(d>2))
        return RPMSG_ERR_PARAM;

      gCtrlLoopChanConfig[nch-1].PID[d-1].deriv_on_PV = ( (((R5_RPMSG_TYPE*)data)->data[2]) != 0);
      break;

    // readback PID command inversion
    case RPMSGCMD_READ_PID_INVCMD:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      d=(int)(((R5_RPMSG_TYPE*)data)->data[1]);
      if((d<1)||(d>2))
        return RPMSG_ERR_PARAM;

      gCtrlLoopChanConfig[nch-1].PID[d-1].invert_cmd = ( (((R5_RPMSG_TYPE*)data)->data[2]) != 0);
      break;

    // readback PID measurement input inversion
    case RPMSGCMD_READ_PID_INVMEAS:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      d=(int)(((R5_RPMSG_TYPE*)data)->data[1]);
      if((d<1)||(d>2))
        return RPMSG_ERR_PARAM;

      gCtrlLoopChanConfig[nch-1].PID[d-1].invert_meas = ( (((R5_RPMSG_TYPE*)data)->data[2]) != 0);
      break;

    // readback trigger state
    case RPMSGCMD_READ_TRIG:
      gRecorderConfig.state=(int)( ((R5_RPMSG_TYPE*)data)->data[0] );
      break;
    
    // readback trigger setup
    case RPMSGCMD_READ_TRIG_CFG:
      nch=(int)(((R5_RPMSG_TYPE*)data)->data[0]);
      if((nch<1)||(nch>4))
        return RPMSG_ERR_PARAM;
      else
        gRecorderConfig.trig_chan=nch;
      
      gRecorderConfig.mode=(int)(((R5_RPMSG_TYPE*)data)->data[1]);
      // if trigger mode is SLOPE, we need to read the other parameters
      if(gRecorderConfig.mode == RECORDER_SLOPE)
        {
        gRecorderConfig.slopedir = (int)(((R5_RPMSG_TYPE*)data)->data[2]);
        // read floating point values directly as float (32 bit)
        memcpy(&(gRecorderConfig.level), &(((R5_RPMSG_TYPE*)data)->data[3]), sizeof(u32));
        }
      break;

    }

  return RPMSG_SUCCESS;
  }


// -----------------------------------------------------------

static void rpmsg_service_unbind(struct rpmsg_endpoint *ept)
  {
  (void)ept;
  rpmsg_destroy_ept(&endp);
  LPRINTF("rpmsg service is destroyed\n\r");
  ept_deleted = 1;
  }

static void rpmsg_name_service_bind_cb(struct rpmsg_device *rdev, const char *name, uint32_t dest)
  {
  LPRINTF("new endpoint notification is received.\n\r");
  if (strcmp(name, RPMSG_SERVICE_NAME))
    LPERROR("Unexpected name service %s.\n\r", name);
  else
    (void)rpmsg_create_ept(&endp, rdev, 
                           RPMSG_SERVICE_NAME, RPMSG_ADDR_ANY, dest, 
                           rpmsg_endpoint_cb, rpmsg_service_unbind);
  }


// -----------------------------------------------------------

static struct remoteproc *SetupRpmsg(int proc_index, int rsc_index)
  {
  int status;
  void *rsc_table;
  int rsc_size;
  metal_phys_addr_t pa;

  (void)proc_index;    // avoid warning on unused parameter
  (void)rsc_index;     // avoid warning on unused parameter
  rsc_size = RSC_MEM_SIZE;
  // init remoteproc instance
  if(!remoteproc_init(&rproc_inst, &rproc_ops, &rproc_priv))
    return NULL;
  LPRINTF("Remoteproc successfully initialized\n\r");
  // mmap resource table
  pa = RSC_MEM_PA;
  LPRINTF("Calling mmap resource table.\n\r");
  rsc_table = remoteproc_mmap(&rproc_inst, &pa, NULL, rsc_size, 0, NULL);
  if(!rsc_table)
    {
    LPRINTF("ERROR: Failed to mmap resource table.\n\r");
    return NULL;
    }
  LPRINTF("Resource table successfully mmaped\n\r");
  // parse resource table to remoteproc
  status = remoteproc_set_rsc_table(&rproc_inst, rsc_table, rsc_size);
  if(status!=0) 
    {
    LPRINTF("Failed to initialize remoteproc\n\r");
    remoteproc_remove(&rproc_inst);
    return NULL;
    }
  LPRINTF("Successfully set resource table to remoteproc\n\r");

  return &rproc_inst;
  }


// -----------------------------------------------------------

int SetupSystem(void **platformp)
  {
  int status, max_size;
  struct remoteproc *rproc;
  struct metal_init_params metal_param = METAL_INIT_DEFAULTS;

  metal_param.log_level = METAL_LOG_INFO;

  if(!platformp)
    {
    LPRINTF("NULL platform pointer -");
    return -EINVAL;
    }

  metal_init(&metal_param);
  
  rproc = SetupRpmsg(0,0);
  if(!rproc)
    return -1;
  
  *platformp = rproc;

  rpdev = create_rpmsg_vdev(rproc, 0, VIRTIO_DEV_DRIVER, NULL, rpmsg_name_service_bind_cb);
  if(!rpdev)
    {
    LPERROR("Failed to create rpmsg virtio device.\n\r");
    return -1;
    }
  
  LPRINTF("Allocating msg buffer\n\r");
  max_size = rpmsg_virtio_get_buffer_size(rpdev);
  if(max_size < sizeof(R5_RPMSG_TYPE))
    {
    LPERROR("too small buffer size.\r\n");
    return -1;
    }

  gMsgPtr = (R5_RPMSG_TYPE *)metal_allocate_memory(sizeof(R5_RPMSG_TYPE));

  if(!gMsgPtr)
    {
    LPERROR("msg buffer allocation failed.\n\r");
    return -1;
    }

  LPRINTF("Try to create rpmsg endpoint.\n\r");
  status = rpmsg_create_ept(&endp, rpdev, RPMSG_SERVICE_NAME,
                            RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
                            rpmsg_endpoint_cb,
                            rpmsg_service_unbind);
  if(status)
    {
    LPERROR("Failed to create endpoint.\n\r");
    metal_free_memory(gMsgPtr);
    return -1;
    }
  LPRINTF("Successfully created rpmsg endpoint.\n\r");

  LPRINTF("Waiting for remote answer...\n\r");
	while(!is_rpmsg_ept_ready(&endp))
		platform_poll(*platformp);

	LPRINTF("RPMSG endpoint is binded with remote.\r\n");

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
  
  return 0;
  }


// -----------------------------------------------------------

int CleanupSystem(void *platform)
  {
  struct remoteproc *rproc = platform;

  rpmsg_destroy_ept(&endp);
  metal_free_memory(gMsgPtr);
  release_rpmsg_vdev(rpdev, platform);
  if(rproc)
    remoteproc_remove(rproc);
  ReleaseSampleShmem();
  metal_finish();
  return 0;
  }


// -----------------------------------------------------------

void InitVars(void)
  {
  int i,j;

  gR5ctrlState=R5CTRLR_IDLE;
  gIncomingRpmsgs=0;
  gFsampl=DEFAULT_TIMER_FREQ_HZ;  // 10 kHz default Fsampling
  gRecorderConfig.state=RECORDER_IDLE;
  gRecorderConfig.trig_chan=1;
  gRecorderConfig.mode=RECORDER_SLOPE;
  gRecorderConfig.slopedir=RECORDER_SLOPE_RISING;
  gRecorderConfig.level=0.;

  for (i=0; i<4; i++)
    {
    g_adcval[i]=0;
    g_dacval[i]=0;
    gADC_offs_cnt[i]=0;
    gADC_gain[i]=1;
    gDAC_offs_cnt[i]=0;

    gWavegenChanConfig[i].state  = WGEN_CH_STATE_OFF;
    gWavegenChanConfig[i].type   = WGEN_CH_TYPE_DC;
    gWavegenChanConfig[i].ampl   = 0.;
    gWavegenChanConfig[i].offs   = 0.;
    gWavegenChanConfig[i].f1     = 0.;
    gWavegenChanConfig[i].f2     = 0.;
    gWavegenChanConfig[i].dt     = 1.;

    // control loop params
  
    gCtrlLoopChanConfig[i].state=CTRLLOOP_CH_DISABLED;

    for(j=0; j<5; j++)
      {
      gCtrlLoopChanConfig[i].input_MISO_A[j]=0.;
      gCtrlLoopChanConfig[i].input_MISO_B[j]=0.;
      gCtrlLoopChanConfig[i].input_MISO_C[j]=0.;
      gCtrlLoopChanConfig[i].input_MISO_D[j]=0.;
      gCtrlLoopChanConfig[i].output_MISO_E[j]=0.;
      gCtrlLoopChanConfig[i].output_MISO_F[j]=0.;
      }

    // identity matrix
    gCtrlLoopChanConfig[i].input_MISO_A[i+1]=1.;
    gCtrlLoopChanConfig[i].input_MISO_B[0]=1.;
    gCtrlLoopChanConfig[i].input_MISO_C[i+1]=1.;
    gCtrlLoopChanConfig[i].input_MISO_D[0]=1.;
    gCtrlLoopChanConfig[i].output_MISO_E[i+1]=1.;
    gCtrlLoopChanConfig[i].output_MISO_F[0]=1.;
  
    gCtrlLoopChanConfig[i].inputSelect=i;
    gDAC_outputSelect[i]=i;
  
    for(j=0; j<2; j++)
      {
      gCtrlLoopChanConfig[i].PID[j].Gp         =1.;
      gCtrlLoopChanConfig[i].PID[j].Gi         =0.;
      gCtrlLoopChanConfig[i].PID[j].G1d        =0.;
      gCtrlLoopChanConfig[i].PID[j].G2d        =0.;
      gCtrlLoopChanConfig[i].PID[j].G_aiw      =1.;
      gCtrlLoopChanConfig[i].PID[j].out_sat    =1.;
      gCtrlLoopChanConfig[i].PID[j].in_thr     =0.;
      gCtrlLoopChanConfig[i].PID[j].deriv_on_PV=false;
      gCtrlLoopChanConfig[i].PID[j].invert_cmd =false;
      gCtrlLoopChanConfig[i].PID[j].invert_meas=false;
      gCtrlLoopChanConfig[i].PID[j].xn1        =0.;
      gCtrlLoopChanConfig[i].PID[j].measn1     =0.;
      gCtrlLoopChanConfig[i].PID[j].yi_n1      =0.;
      gCtrlLoopChanConfig[i].PID[j].yd_n1      =0.;

      gCtrlLoopChanConfig[i].IIR[j].a[0]  =1.;
      gCtrlLoopChanConfig[i].IIR[j].a[1]  =0.;
      gCtrlLoopChanConfig[i].IIR[j].a[2]  =0.;
      gCtrlLoopChanConfig[i].IIR[j].b[0]  =0.;
      gCtrlLoopChanConfig[i].IIR[j].b[1]  =0.;
      gCtrlLoopChanConfig[i].IIR[j].x[0]  =0.;
      gCtrlLoopChanConfig[i].IIR[j].x[1]  =0.;
      gCtrlLoopChanConfig[i].IIR[j].y[0]  =0.;
      gCtrlLoopChanConfig[i].IIR[j].y[1]  =0.;
      }
    
    }
  }

// -----------------------------------------------------------

int main(int argc, char *argv[])
  {
  int status, numbytes, rpmsglen;
//  int theAmp;
//  float theFreq, theVolt;
  unsigned long numIRQ;
  int sock, maxfd, newfd;
  int i, nready;
  fd_set active_fd_set, read_fd_set;
  struct sockaddr_in clientname;
  size_t size;
  
  // remove buffering from stdin and stdout
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stdin,  NULL, _IONBF, 0);

  LPRINTF("\r\nR5 test application #5 : shared PL resources + IRQs + IPC (openamp)\r\n\r\n");
  LPRINTF("This is A53/linux side\r\n\r\n");

  InitVars();

  status = SetupSystem(&gplatform);
  if(status!=0)
    {
    LPRINTF("ERROR Setting up System - aborting\r\n");
    return status;
    }
  
  status = startSCPIserver(&sock, &active_fd_set);
  if(status!=SCPI_NO_ERR)
    {
    LPRINTF("ERROR Setting up SCPI server - aborting\r\n");
    return status;
    }
  maxfd = sock;
  rpmsglen=sizeof(R5_RPMSG_TYPE);

  // main loop

  while(1)
    {
    // block until input arrives on one or more active sockets
    //LPRINTF("Listening\r\n");
    read_fd_set = active_fd_set;
    nready=select(maxfd+1, &read_fd_set, NULL, NULL, NULL);
    if(nready<0)
      {
      LPRINTF("select() error\r\n");
      }
    else
      {
      // service all the sockets with input pending
      for(i=0; i<=maxfd && nready>0; i++)
        {
        if(FD_ISSET(i, &read_fd_set))
          {
          nready--;
          if(i == sock)
            {
            // new connection request on original socket
            size = sizeof(clientname);
            newfd = accept(sock,
                          (struct sockaddr *) &clientname,
                          (socklen_t * restrict) &size);
            if(newfd < 0)
              {
              LPRINTF("accept() error\r\n");
              }
            else
              {
              LPRINTF("SCPI server: new connection from host %s, port %hd\r\n",
                     inet_ntoa(clientname.sin_addr),
                     ntohs(clientname.sin_port));
              FD_SET(newfd, &active_fd_set);
              if(newfd>maxfd)
                {
                maxfd=newfd;
                }
              }
            }    // if new connection
            else
            {
            // data arriving on an already-connected socket
            //fprintf(stderr,"Incoming data\n");
            if(SCPI_read_from_client(i, &endp, gMsgPtr) < 0)
              {
              LPRINTF("Closing connection\r\n");
              close(i);
              FD_CLR(i, &active_fd_set);
              // I don't update maxfd; I should loop on the fd set to find the new maximum: not worth
              }
            }    // if data from already-connected client
          }    // if input pending
        }    // loop on FD set
      }    // if not select() error
    }    // while 1



  // while(1)
  //   {
  //   // for debug, read number of timer IRQ from ADCsample shared memory offset 0 and print it
  //   numIRQ=metal_io_read32(sample_shmem_io, 0);
  //   LPRINTF("# of Timer IRQs: %d\r\n", numIRQ);

  //   }
  
  // cleanup and exit
  LPRINTF("\n\rExiting\n\r");

  status = CleanupSystem(gplatform);
  if(status!=0)
    {
    LPRINTF("ERROR Cleaning up System\n\r");
    // continue anyway, as we are shutting down
    //return status;
    }

  return 0;
  }
