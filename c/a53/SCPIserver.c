//
// R5 integrated controller 
// ZCU102 + ADI CN0585/CN0584
//
// IRQs from PL and 
// interproc communications with A53/linux
//
//////////////////////////////////////////
//
// this is the linux SCPI server
//

#include "SCPIserver.h"

// ##########  globals  #######################
 
// ##########  implementation  ################
 
void upstring(char *s)
  {
  char *p;
  for(p=s; *p; p++)
    *p=toupper((unsigned char) *p);
  }


//-------------------------------------------------------------------

void trimstring(char* s)
  {
  char   *begin, *end;
  char   wbuf[SCPI_MAXMSG+1];
  size_t len;

  len=strlen(s);
  if(!len)
    return;
  
  for(end=s+len-1; end >=s && isspace(*end); end--)
    ;
  for(begin=s; begin <=end && isspace(*begin); begin++)
    ;
  
  len=end-begin+1;
  // must use memcpy because I don't have proper 0-termination yet
  memcpy(wbuf, begin, len);
  wbuf[len]=0;
  // I can use strcpy as now I have proper 0-termination
  (void) strcpy(s, wbuf);
  }


//-------------------------------------------------------------------

void parse_IDN(char *ans, size_t maxlen)
  {
  FILE *fd;
  char prod[SCPI_MAXMSG+1], ver[SCPI_MAXMSG+1];
  char *s;

  // firmware name
  fd = fopen(PRODUCT_FNAME, "r");
  if(fd!=NULL)
    {
    s=fgets(prod, SCPI_MAXMSG, fd);
    fclose(fd);
    if(s==NULL)
      strcpy(prod,"Unknown Firmware");
    }
  else
    strcpy(prod,"Unknown Firmware");
  
  // firmware version
  fd = fopen(VERSION_FNAME, "r");
  if(fd!=NULL)
    {
    s=fgets(ver, SCPI_MAXMSG, fd);
    fclose(fd);
    if(s==NULL)
      strcpy(ver,"Unknown");
    }
  else
    strcpy(ver,"Unknown");
  
  trimstring(prod);
  trimstring(ver);
  snprintf(ans, maxlen, "%s: %s - version %s\n", SCPI_OKS, prod, ver);
  }


//-------------------------------------------------------------------

void parse_STB(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  int numbytes, rpmsglen, status;

  // retrieve R5 state

  if(rw==SCPI_READ)
    {
    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_STATE;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: STB READ rpmsg_send() failed\n", SCPI_ERRS);
    else
      {
      // now wait for answer
      status=WaitForRpmsg();
      switch(status)
        {
        case RPMSG_ANSWER_VALID:
          // print current state
          snprintf(ans, maxlen, "%s: %d is the status word\n", SCPI_OKS, gR5ctrlState);
          break;
        case RPMSG_ANSWER_TIMEOUT:
          snprintf(ans, maxlen, "%s: STATUS READ timed out\n", SCPI_ERRS);
          break;
        case RPMSG_ANSWER_ERR:
          snprintf(ans, maxlen, "%s: STATUS READ error\n", SCPI_ERRS);
          break;
        }
      }
    }
  else
    {
    snprintf(ans, maxlen, "%s: write operation not supported\n", SCPI_ERRS);
    }
  }


//-------------------------------------------------------------------

void parse_RST(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  int numbytes, rpmsglen, status;
  
  if(rw==SCPI_READ)
    {
    snprintf(ans, maxlen, "%s: read operation not supported\n", SCPI_ERRS);        
    }
  else
    {
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_RESET;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: RST rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s: controller reset\n", SCPI_OKS);
    }
  }


//-------------------------------------------------------------------

void parse_DAC(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int n,nch, dac[4];
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_DAC;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: DAC READBACK rpmsg_send() failed\n", SCPI_ERRS);
    else
      {
      // now wait for answer
      status=WaitForRpmsg();
      switch(status)
        {
        case RPMSG_ANSWER_VALID:
          snprintf(ans, maxlen, "%s: %d %d %d %d\n", SCPI_OKS, g_dacval[0], g_dacval[1], g_dacval[2], g_dacval[3]);
          break;
        case RPMSG_ANSWER_TIMEOUT:
          snprintf(ans, maxlen, "%s: DAC READBACK timed out\n", SCPI_ERRS);
          break;
        case RPMSG_ANSWER_ERR:
          snprintf(ans, maxlen, "%s: DAC READBACK error\n", SCPI_ERRS);
          break;
        }
      }
    }
  else
    {
    // parse the values to write into the 4 DAC channels;
    // format is 16 bit 2's complement int, written in decimal
    nch=0;
    do
      {
      // next in line is the desired value for nex DAC channel
      p=strtok(NULL," ");
      if(p!=NULL)
        {
        n=(int)strtol(p, NULL, 10);
        if(errno!=0 && n==0)
          {
          snprintf(ans, maxlen, "%s: invalid DAC value\n", SCPI_ERRS);
          p=NULL;     // to break out of do..while cycle
          }
        else
          {
          dac[nch++]=n;
          }
        }      
      } 
    while((p!=NULL) && (nch<4));

    // now send the new values to R5
    if((p!=NULL) && (nch=4))
      {
      rpmsglen=sizeof(R5_RPMSG_TYPE);
      rpmsg_ptr->command = RPMSGCMD_WRITE_DAC;
      rpmsg_ptr->data[0]=((u32)(dac[1])<<16)&0xFFFF0000 | ((u32)(dac[0]))&0x0000FFFF;
      rpmsg_ptr->data[1]=((u32)(dac[3])<<16)&0xFFFF0000 | ((u32)(dac[2]))&0x0000FFFF;
      numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
      if(numbytes<rpmsglen)
        snprintf(ans, maxlen, "%s: DAC WRITE rpmsg_send() failed\n", SCPI_ERRS);
      else
        snprintf(ans, maxlen, "%s: DAC updated (%d %d %d %d)\n", SCPI_OKS, dac[0], dac[1], dac[2], dac[3]);
      }
    else
      {
      snprintf(ans, maxlen, "%s: missing DAC channel value\n", SCPI_ERRS);
      }
    }
  }


//-------------------------------------------------------------------

void parse_DACCH(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch,dacvalue;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    snprintf(ans, maxlen, "%s: read operation not supported; use DAC? instead\n", SCPI_ERRS);
    }
  else
    {
    // parse the values to write into one specific DAC channel;
    // format is 16 bit 2's complement int, written in decimal

    // next in line is the desired DAC channel
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      nch=(int)strtol(p, NULL, 10);
      if(errno!=0 && nch==0)
        {
        snprintf(ans, maxlen, "%s: invalid DAC channel number\n", SCPI_ERRS);
        }
      else
        {
        if((nch<1)||(nch>4))
          {
          snprintf(ans, maxlen, "%s: DAC channel out of range [1..4]\n", SCPI_ERRS);
          }
        else
          {
          // next in line is the DAC value
          p=strtok(NULL," ");
          if(p!=NULL)
            {
            dacvalue=(int)strtol(p, NULL, 10);
            if(errno!=0 && dacvalue==0)
              {
              snprintf(ans, maxlen, "%s: invalid DAC value\n", SCPI_ERRS);
              }
            else
              {
              // everything is ready: send rpmsg to R5
              rpmsglen=sizeof(R5_RPMSG_TYPE);
              rpmsg_ptr->command = RPMSGCMD_WRITE_DACCH;
              rpmsg_ptr->data[0]=((u32)(nch)<<16)&0xFFFF0000 | ((u32)(dacvalue))&0x0000FFFF;
              numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
              if(numbytes<rpmsglen)
                snprintf(ans, maxlen, "%s: DACCH WRITE rpmsg_send() failed\n", SCPI_ERRS);
              else
                snprintf(ans, maxlen, "%s: DAC CH %d updated with value %d\n", SCPI_OKS, nch, dacvalue);
              }
            }
          else
            {
            snprintf(ans, maxlen, "%s: missing DAC value\n", SCPI_ERRS);
            }
          }
        }
      }
    else
      {
      snprintf(ans, maxlen, "%s: missing DAC channel\n", SCPI_ERRS);
      }
    }
  }


//-------------------------------------------------------------------

void parse_READ_ADC(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  int n,nch, adc[4];
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_ADC;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: ADC READ rpmsg_send() failed\n", SCPI_ERRS);
    else
      {
      // now wait for answer
      status=WaitForRpmsg();
      switch(status)
        {
        case RPMSG_ANSWER_VALID:
          snprintf(ans, maxlen, "%s: %d %d %d %d\n", SCPI_OKS, g_adcval[0], g_adcval[1], g_adcval[2], g_adcval[3]);
          break;
        case RPMSG_ANSWER_TIMEOUT:
          snprintf(ans, maxlen, "%s: ADC READ timed out\n", SCPI_ERRS);
          break;
        case RPMSG_ANSWER_ERR:
          snprintf(ans, maxlen, "%s: ADC READ error\n", SCPI_ERRS);
          break;
        }
      }
    }
  else
    {
    snprintf(ans, maxlen, "%s: write operation not supported\n", SCPI_ERRS);
    }
  }


//-------------------------------------------------------------------

void parse_FSAMPL(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  u32 fs;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_FSAMPL;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: FSAMPL READ rpmsg_send() failed\n", SCPI_ERRS);
    else
      {
      // now wait for answer
      status=WaitForRpmsg();
      switch(status)
        {
        case RPMSG_ANSWER_VALID:
          snprintf(ans, maxlen, "%s: %d Hz\n", SCPI_OKS, gFsampl);
          break;
        case RPMSG_ANSWER_TIMEOUT:
          snprintf(ans, maxlen, "%s: FSAMPL READ timed out\n", SCPI_ERRS);
          break;
        case RPMSG_ANSWER_ERR:
          snprintf(ans, maxlen, "%s: FSAMPL READ error\n", SCPI_ERRS);
          break;
        }
      }
    }
  else
    {
    // parse the desired sampling frequency in Hz
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      fs=(u32)strtol(p, NULL, 10);
      if(errno!=0 && fs==0)
        {
        snprintf(ans, maxlen, "%s: invalid sampling frequency\n", SCPI_ERRS);
        }
      else
        {
        // send rpmsg to R5
        rpmsglen=sizeof(R5_RPMSG_TYPE);
        rpmsg_ptr->command = RPMSGCMD_WRITE_FSAMPL;
        rpmsg_ptr->data[0]=fs;
        numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
        if(numbytes<rpmsglen)
          snprintf(ans, maxlen, "%s: FSAMPL WRITE rpmsg_send() failed\n", SCPI_ERRS);
        else
          snprintf(ans, maxlen, "%s: requested %d Hz sampling frequency\n", SCPI_OKS, fs);
        }
      }
    else
      {
      snprintf(ans, maxlen, "%s: missing sampling frequency\n", SCPI_ERRS);
      }
    }
  }


//-------------------------------------------------------------------

void parse_WAVEGEN(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int onoff;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    snprintf(ans, maxlen, "%s: read operation not supported; use *STB? instead\n", SCPI_ERRS);
    }
  else
    {
    // turn wave generator mode ON or OFF

    // next in line is ON or OFF
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      if(strcmp(p,"ON")==0)
        onoff=1;
      else if(strcmp(p,"OFF")==0)
        onoff=0;
      else
        onoff=-1;
      
      if(onoff>=0)
        {
        // send rpmsg to R5
        rpmsglen=sizeof(R5_RPMSG_TYPE);
        rpmsg_ptr->command = RPMSGCMD_WGEN_ONOFF;
        rpmsg_ptr->data[0]=(u32)onoff;
        numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
        if(numbytes<rpmsglen)
          snprintf(ans, maxlen, "%s: WAVEGEN WRITE rpmsg_send() failed\n", SCPI_ERRS);
        else
          snprintf(ans, maxlen, "%s: WAVEFORM GENERATOR switched %s\n", SCPI_OKS, onoff==1? "ON" : "OFF");
        }
      else
        {
        snprintf(ans, maxlen, "%s: use ON/OFF with WAVEGEN command\n", SCPI_ERRS);
        }        
      }
    else
      {
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", SCPI_ERRS);
      }
    }
  }


//-------------------------------------------------------------------

void parse_WAVEGEN_CH_CONFIG(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch, wtype;
  float x;
  int numbytes, rpmsglen, status;
  char wtypestr[16];

  
  if(rw==SCPI_READ)
    {
    // READ: request waveform generator channel configuration

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing WAVEGEN channel number\n", SCPI_ERRS);
      return;
      }
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid WAVEGEN channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: WAVEGEN channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_WGEN_CH_CONF;
    rpmsg_ptr->data[0] = (u32)(nch);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: WAVEGEN CHAN CONF READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        switch(gWavegenChanConfig[nch-1].type)
          {
          case WGEN_CH_TYPE_DC:
            strcpy(wtypestr,"DC");
            break;
          case WGEN_CH_TYPE_SINE:
            strcpy(wtypestr,"SINE");
            break;
          case WGEN_CH_TYPE_SWEEP:
            strcpy(wtypestr,"SWEEP");
            break;
          }

        snprintf(ans, maxlen, "%s: %s %g %g %g %g %g\n",
                    SCPI_OKS,
                    wtypestr,
                    gWavegenChanConfig[nch-1].ampl,
                    gWavegenChanConfig[nch-1].offs,
                    gWavegenChanConfig[nch-1].f1,
                    gWavegenChanConfig[nch-1].f2,
                    gWavegenChanConfig[nch-1].dt
                );
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: WAVEGEN CH CONFIG READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: WAVEGEN CH CONFIG READ error\n", SCPI_ERRS);
        break;
      }


    }
  else
    {
    // WRITE: configure one waveform generator channel

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing WAVEGEN channel number\n", SCPI_ERRS);
      return;
      }
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid WAVEGEN channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: WAVEGEN channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }


    // next in line is the waveform TYPE (DC/SINE/SWEEP)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing waveform type\n", SCPI_ERRS);
      return;
      }
    if(strcmp(p,"DC")==0)
      wtype=WGEN_CH_TYPE_DC;
    else if(strcmp(p,"SINE")==0)
      wtype=WGEN_CH_TYPE_SINE;
    else if(strcmp(p,"SWEEP")==0)
      wtype=WGEN_CH_TYPE_SWEEP;
    else
      wtype=-1;
    
    if(wtype<0)
      {
      snprintf(ans, maxlen, "%s: use DC/SINE/SWEEP as waveform type\n", SCPI_ERRS);
      return;
      }

    // start filling the rpmsg with the integer parameters
    rpmsg_ptr->command = RPMSGCMD_WRITE_WGEN_CH_CONF;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(wtype);

    // now parse floating point values; 
    // the number of parameters depends on the type of waveform that was requested


    // next in line is amplitude (needed by every waveform)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing amplitude\n", SCPI_ERRS);
      return;
      }
    x=strtof(p, NULL);
    // error check with float does not work
    // if(errno!=0 && errno!=EIO && x==0.0F)
    //   {
    //   snprintf(ans, maxlen, "%s: invalid amplitude\n", SCPI_ERRS);
    //   return;
    //   }
    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[2]), &x, sizeof(u32));


    // next in line is offset (for SINE and SWEEP waveforms, not for DC)
    if(wtype!=WGEN_CH_TYPE_DC)
      {
      p=strtok(NULL," ");
      if(p==NULL)
        {
        snprintf(ans, maxlen, "%s: missing offset\n", SCPI_ERRS);
        return;
        }
      x=strtof(p, NULL);
      // 0.0 is an acceptable value for the offset;
      // error check with float does not work
      // if(errno!=0 && errno!=EIO && x==0.0F)
      //   {
      //   snprintf(ans, maxlen, "%s: invalid offset (%g; errno=%d)\n", SCPI_ERRS,x,errno);
      //   return;
      //   }
      }
    else
      x=0.0F;

    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[3]), &x, sizeof(u32));
    //LPRINTF(">>%g<<\n",*((float*)&(rpmsg_ptr->data[3])));


    // next in line is f1 (for SINE and SWEEP waveforms, not for DC)
    if(wtype!=WGEN_CH_TYPE_DC)
      {
      p=strtok(NULL," ");
      if(p==NULL)
        {
        snprintf(ans, maxlen, "%s: missing frequency\n", SCPI_ERRS);
        return;
        }
      x=strtof(p, NULL);
      // error check with float does not work
      // if(errno!=0 && errno!=EIO && x==0.0F)
      //   {
      //   snprintf(ans, maxlen, "%s: invalid frequency\n", SCPI_ERRS);
      //   return;
      //   }
      }
    else
      x=0.0F;

    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[4]), &x, sizeof(u32));


    // next in line is f2 (for SWEEP waveform only, not for DC or SINE)
    if(wtype==WGEN_CH_TYPE_SWEEP)
      {
      p=strtok(NULL," ");
      if(p==NULL)
        {
        snprintf(ans, maxlen, "%s: missing end frequency\n", SCPI_ERRS);
        return;
        }
      x=strtof(p, NULL);
      // error check with float does not work
      // if(errno!=0 && errno!=EIO && x==0.0F)
      //   {
      //   snprintf(ans, maxlen, "%s: invalid end frequency\n", SCPI_ERRS);
      //   return;
      //   }
      }
    else
      x=0.0F;

    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[5]), &x, sizeof(u32));


    // next in line is dt (for SWEEP waveform only, not for DC or SINE)
    if(wtype==WGEN_CH_TYPE_SWEEP)
      {
      p=strtok(NULL," ");
      if(p==NULL)
        {
        snprintf(ans, maxlen, "%s: missing sweep time\n", SCPI_ERRS);
        return;
        }
      x=strtof(p, NULL);
      // error check with float does not work
      // if(errno!=0 && errno!=EIO && x==0.0F)
      //   {
      //   snprintf(ans, maxlen, "%s: invalid sweep time\n", SCPI_ERRS);
      //   return;
      //   }
      }
    else
      x=1.0F;  // to prevent divisions by 0 :-)

    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[6]), &x, sizeof(u32));


    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: WAVEGEN CH CONF WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s: updated config for WAVEGEN chan %d\n", SCPI_OKS, nch);
    }

  }


//-------------------------------------------------------------------

void parse_WAVEGEN_CH_ENABLE(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch,enbl;
  int numbytes, rpmsglen, status;
  char enblstr[16];

  
  if(rw==SCPI_READ)
    {
    // READ: request waveform generator channel ON/OFF state

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing WAVEGEN channel number\n", SCPI_ERRS);
      return;
      }
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid WAVEGEN channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: WAVEGEN channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_WGEN_CH_EN;
    rpmsg_ptr->data[0] = (u32)(nch);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: WAVEGEN CHAN EN READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:

        switch(gWavegenChanConfig[nch-1].enable)
          {
          case WGEN_CH_ENABLE_OFF:
            strcpy(enblstr,"OFF");
            break;
          case WGEN_CH_ENABLE_ON:
            strcpy(enblstr,"ON");
            break;
          case WGEN_CH_ENABLE_SINGLE:
            strcpy(enblstr,"SINGLE");
            break;
          }

        snprintf(ans, maxlen, "%s: %s\n", SCPI_OKS, enblstr);
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: WAVEGEN CH ENABLE READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: WAVEGEN CH ENABLE READ error\n", SCPI_ERRS);
        break;
      }

    }
  else
    {
    // WRITE: enable/disable one waveform generator channel

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing WAVEGEN channel number\n", SCPI_ERRS);
      return;
      }
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid WAVEGEN channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: WAVEGEN channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the ENABLE state (OFF/ON/SINGLE)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", SCPI_ERRS);
      return;
      }
    if(strcmp(p,"OFF")==0)
      enbl=WGEN_CH_ENABLE_OFF;
    else if(strcmp(p,"ON")==0)
      enbl=WGEN_CH_ENABLE_ON;
    else if(strcmp(p,"SINGLE")==0)
      enbl=WGEN_CH_ENABLE_SINGLE;
    else
      enbl=-1;
    
    if(enbl<0)
      {
      snprintf(ans, maxlen, "%s: use ON/OFF/SINGLE for WAVEGEN channel enable state\n", SCPI_ERRS);
      return;
      }


    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_WRITE_WGEN_CH_EN;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(enbl);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: WAVEGEN CH EN WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s: WAVEGEN chan %d is now %s\n", SCPI_OKS, nch, p);
    }

  }


//-------------------------------------------------------------------

void parse_TRIG(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int trigstate;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_TRIG;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: TRIG READ rpmsg_send() failed\n", SCPI_ERRS);
    else
      {
      // now wait for answer
      status=WaitForRpmsg();
      switch(status)
        {
        case RPMSG_ANSWER_VALID:
          // print current state
          switch(gRecorderConfig.state)
            {
            case RECORDER_IDLE:
              snprintf(ans, maxlen, "%s: IDLE is recorder state\n", SCPI_OKS);
              break;
            case RECORDER_FORCE:
            case RECORDER_ARMED:
              snprintf(ans, maxlen, "%s: ARMED is recorder state\n", SCPI_OKS);
              break;
            case RECORDER_ACQUIRING:
              snprintf(ans, maxlen, "%s: ACQUIRING is recorder state\n", SCPI_OKS);
              break;
            }
          break;
        case RPMSG_ANSWER_TIMEOUT:
          snprintf(ans, maxlen, "%s: TRIG READ timed out\n", SCPI_ERRS);
          break;
        case RPMSG_ANSWER_ERR:
          snprintf(ans, maxlen, "%s: TRIG READ error\n", SCPI_ERRS);
          break;
        }
      }    
    }
  else
    {
    // ARM/DISARM trigger

    // next in line is ARM/FORCE/OFF
    p=strtok(NULL," ");
    if(p!=NULL)
      {
      if(strcmp(p,"ARM")==0)
        trigstate=RECORDER_ARMED;
      else if(strcmp(p,"FORCE")==0)
        trigstate=RECORDER_FORCE;
      else if(strcmp(p,"OFF")==0)
        trigstate=RECORDER_IDLE;
      else
        trigstate=-1;
      
      if(trigstate>=0)
        {
        // send rpmsg to R5
        rpmsglen=sizeof(R5_RPMSG_TYPE);
        rpmsg_ptr->command = RPMSGCMD_WRITE_TRIG;
        rpmsg_ptr->data[0]=(u32)trigstate;
        numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
        if(numbytes<rpmsglen)
          snprintf(ans, maxlen, "%s: TRIG WRITE rpmsg_send() failed\n", SCPI_ERRS);
        else
          snprintf(ans, maxlen, "%s: TRIGGER state updated\n", SCPI_OKS);
        }
      else
        {
        snprintf(ans, maxlen, "%s: use ARM/FORCE/OFF with RECORD:TRIG command\n", SCPI_ERRS);
        }        
      }
    else
      {
      snprintf(ans, maxlen, "%s: missing ARM/FORCE/OFF option\n", SCPI_ERRS);
      }
    }
  }


//-------------------------------------------------------------------

void parse_TRIGSETUP(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int trigch, trigmode,slope;
  float lvl;
  int numbytes, rpmsglen, status;
  char modestr[16],slopestr[16];

  
  if(rw==SCPI_READ)
    {
    // READ: request trigger setup

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_TRIG_CFG;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: TRIG setup READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        switch(gRecorderConfig.mode)
          {
          case RECORDER_SWEEP:
            strcpy(modestr,"SWEEP");
            break;
          case RECORDER_SLOPE:
            strcpy(modestr,"SLOPE");
            break;
          }
        if(gRecorderConfig.mode == RECORDER_SWEEP)
          {
          // if trig mode is SWEEP, we are done
          snprintf(ans, maxlen, "%s: %d %s\n", SCPI_OKS, gRecorderConfig.trig_chan, modestr);
          }
        else
          {
          // if trig mode is SLOPE, we need to read the other parameters
          switch(gRecorderConfig.slopedir)
            {
            case RECORDER_SLOPE_RISING:
              strcpy(slopestr,"RISING");
              break;
            case RECORDER_SLOPE_FALLING:
              strcpy(slopestr,"FALLING");
              break;
            }
          snprintf(ans, maxlen, "%s: %d %s %s %g\n", SCPI_OKS, 
                                     gRecorderConfig.trig_chan, modestr, slopestr, gRecorderConfig.level);
          }
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: TRIG setup READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: TRIG setup READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE: configure trigger

    // next in line is the trigger channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing TRIG channel number\n", SCPI_ERRS);
      return;
      }
    trigch=(int)strtol(p, NULL, 10);
    if(errno!=0 && trigch==0)
      {
      snprintf(ans, maxlen, "%s: invalid TRIG channel number\n", SCPI_ERRS);
      return;
      }
    if((trigch<1)||(trigch>4))
      {
      snprintf(ans, maxlen, "%s: TRIG channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }


    // next in line is the trigger mode (SLOPE/SWEEP)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing TRIG MODE (SLOPE/SWEEP)\n", SCPI_ERRS);
      return;
      }
    if(strcmp(p,"SLOPE")==0)
      trigmode=RECORDER_SLOPE;
    else if(strcmp(p,"SWEEP")==0)
      trigmode=RECORDER_SWEEP;
    else
      trigmode=-1;
    
    if(trigmode<0)
      {
      snprintf(ans, maxlen, "%s: use SLOPE/SWEEP as TRIG mode\n", SCPI_ERRS);
      return;
      }

    // start filling the rpmsg with the integer parameters
    rpmsg_ptr->command = RPMSGCMD_WRITE_TRIG_CFG;
    rpmsg_ptr->data[0] = (u32)(trigch);
    rpmsg_ptr->data[1] = (u32)(trigmode);

    if(trigmode==RECORDER_SWEEP)
      {
      // if trig mode is SWEEP, we are done
      rpmsg_ptr->data[2] = 0;
      rpmsg_ptr->data[3] = 0;
      }
    else
      {
      // if trig mode is SLOPE, we need to read the other parameters

      // next in line is slope direction (RISING/FALLING)
      p=strtok(NULL," ");
      if(p==NULL)
        {
        snprintf(ans, maxlen, "%s: missing slope direction (RISING/FALLING)\n", SCPI_ERRS);
        return;
        }
      if(strcmp(p,"RISING")==0)
        slope=RECORDER_SLOPE_RISING;
      else if(strcmp(p,"FALLING")==0)
        slope=RECORDER_SLOPE_FALLING;
      else
        slope=-1;
      
      if(slope<0)
        {
        snprintf(ans, maxlen, "%s: use SLOPE/SWEEP as TRIG mode\n", SCPI_ERRS);
        return;
        }

      rpmsg_ptr->data[2] = (u32)(slope);

      // next in line is trigger level
      p=strtok(NULL," ");
      if(p==NULL)
        {
        snprintf(ans, maxlen, "%s: missing TRIG level\n", SCPI_ERRS);
        return;
        }
      lvl=strtof(p, NULL);
      if(errno!=0 && errno!=EIO && lvl==0.0F)
        {
        snprintf(ans, maxlen, "%s: invalid TRIG level\n", SCPI_ERRS);
        return;
        }
  
      // write floating point values directly as float (32 bit)
      memcpy(&(rpmsg_ptr->data[3]), &lvl, sizeof(u32));
      }

    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: TRIG CONF WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s: updated TRIG configuration\n", SCPI_OKS);
    }

  }


//-------------------------------------------------------------------

void printSamples(int filedes)
  {
  char  s[256];
  unsigned long nsampl;
  int i,j;
  s16 sampl;

  nsampl=metal_io_read32(sample_shmem_io, 0);
  if(nsampl>MAX_SHM_SAMPLES)
    nsampl=MAX_SHM_SAMPLES;
  snprintf(s,256,"%s: %d total samples\n", SCPI_OKS, nsampl);
  sendback(filedes,s);

  for(i=0;i<nsampl; i++)
    {
    for(j=0;j<4; j++)
      {
      sampl=metal_io_read16(sample_shmem_io, SAMPLE_SHM_HEADER_LEN+i*8+j*2);
      snprintf(s,256,"%8d  ", sampl);
      sendback(filedes,s);
      }
    sendback(filedes,"\n");
    }
  }


//-------------------------------------------------------------------

void printHelp(int filedes)
  {
  sendback(filedes,"R5 controller SCPI server commands\n\n");
  sendback(filedes,"Server supports multiple concurrent clients\n");
  sendback(filedes,"Server is case insensitive\n");
  sendback(filedes,"Numbers are decimal\n");
  sendback(filedes,"Server answers with OK or ERR, a colon and a descriptive message\n");
  sendback(filedes,"Send CTRL-C (CTRL-D on windows) to close the connection\n\n");
  sendback(filedes,"Command list:\n\n");
  sendback(filedes,"*IDN?                         : print firmware name and version\n");
  sendback(filedes,"*STB?                         : retrieve current state; answer is bit-coded:\n");
  sendback(filedes,"                                  bit 0  = recorder enabled\n");
  sendback(filedes,"                                  bit 1  = waveform generator enabled\n");
  sendback(filedes,"                                  bit 2  = control loop enabled\n");
  sendback(filedes,"*RST                          : reset controller\n");
  sendback(filedes,"DAC <ch1> <ch2> <ch3> <ch4>   : write value of all 4 DAC channels;\n");
  sendback(filedes,"                                the values are 16-bit 2's complement integers\n");
  sendback(filedes,"                                in decimal notation; note that the values\n");
  sendback(filedes,"                                are actuated synchronously to next edge\n");
  sendback(filedes,"                                of the sampling clock\n");
  sendback(filedes,"DAC?                          : read back the value of all 4 DAC channels;\n");
  sendback(filedes,"DACCH <nch> <val>             : write value <val> into DAC channel <nch>;\n");
  sendback(filedes,"                                <nch> is in range [1..4];\n");
  sendback(filedes,"                                <val> is a 16-bit 2's complement integer\n");
  sendback(filedes,"                                in decimal notation\n");
  sendback(filedes,"ADC?                          : read the values of the 4 ADC channels; note that\n");
  sendback(filedes,"                                the values are updated at every cycle\n");
  sendback(filedes,"                                of the sampling clock\n");
  sendback(filedes,"FSAMPL <val>                  : request to change sampling frequency to <val> Hz;\n");
  sendback(filedes,"                                it will be approximated to the closest possible frequency;\n");
  sendback(filedes,"                                use FSAMPL? to retrieve the actual value set\n");
  sendback(filedes,"FSAMPL?                       : retrieve sampling frequency (1Hz precision)\n");
  sendback(filedes,"WAVEGEN {OFF|ON}              : start/stop waveform generator mode\n");
  sendback(filedes,"WAVEGEN:CH_CONFIG <nch> {DC|SINE|SWEEP} <ampl> <offs> <freq1> <freq2> <sweeptime>\n");
  sendback(filedes,"                              : configures channel <chan> of the waveform generator;\n");
  sendback(filedes,"                                notes:\n");
  sendback(filedes,"                                - <nch> is in range [1..4];\n");
  sendback(filedes,"                                - <ampl> and <offs> are fraction of the fullscale, so\n");
  sendback(filedes,"                                  <ampl> is in (0,1) and <offs> is in (-1,1) range;\n");
  sendback(filedes,"                                - <freq1> and <freq2> are in Hz;\n");
  sendback(filedes,"                                - <sweeptime> is in seconds\n");
  sendback(filedes,"                                - DC takes only the argument <ampl>;\n");
  sendback(filedes,"                                - SINE takes only the arguments <ampl>, <offs> and <freq1>;\n");
  sendback(filedes,"WAVEGEN:CH_CONFIG? <nch>      : queries the configuration of waveform generator \n");
  sendback(filedes,"                                channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                the answer will be in the form:\n");
  sendback(filedes,"                                {DC|SINE|SWEEP} <ampl> <offs> <freq1> <freq2> <sweeptime>\n");
  sendback(filedes,"WAVEGEN:CH_ENABLE <nch> {OFF|ON|SINGLE}\n");
  sendback(filedes,"                              : enables/disables channel <nch> of the waveform generator;\n");
  sendback(filedes,"                                SINGLE can be used only in association with SWEEP;\n");
  sendback(filedes,"WAVEGEN:CH_ENABLE? <nch>      : retrieves on/off state of channel <nch> of the waveform generator\n");
  sendback(filedes,"RECORD:TRIGger {ARM|FORCE|OFF}: same settings of an oscilloscope trigger:\n");
  sendback(filedes,"                                - ARM   waits for the conditions specified in\n");
  sendback(filedes,"                                        RECORD:TRIG_SETUP to start an acquisition\n");
  sendback(filedes,"                                - FORCE commands an immediate start of the recording\n");
  sendback(filedes,"                                - OFF   stops recording\n");
  sendback(filedes,"RECORD:TRIGger?               : depending on the recorder state, it returns\n");
  sendback(filedes,"                                {ARMED|ACQUIRING|IDLE}\n");
  sendback(filedes,"RECORD:TRIGger:SETUP <trig_ch> SLOPE {RISING|FALLING) <level>\n");
  sendback(filedes,"                              : setup the recorder trigger; <trig_ch> is in range [1..4];\n");
  sendback(filedes,"                                <level> is a fraction of the fullscale, i.e a number <1\n");
  sendback(filedes,"                                in magnitude\n");
  sendback(filedes,"RECORD:TRIGger:SETUP <trig_ch> SWEEP\n");
  sendback(filedes,"                              : the recorder is configured to trigger as soon as the\n");
  sendback(filedes,"                                waveform generator starts a new sweep on channel <trig_ch>.\n");
  sendback(filedes,"                                This mode can be used to draw Bode diagrams of a a system under test\n");
  sendback(filedes,"RECORD:SAMPLES?               : retrieve the recored samples; use after a successful trigger,\n");
  sendback(filedes,"                                when the RECORD:TRIGger? reports an IDLE state, signaling the end\n");
  sendback(filedes,"                                of an acquisition.\n");
  sendback(filedes,"                                The answer is a first line with the total number of samples,\n");
  sendback(filedes,"                                then one line per sample with all four ADC channels printed\n");
  sendback(filedes,"                                as decimal 16-bit signed integers; first value is first ADC channel\n");
  sendback(filedes,"                                NOTE: when recording in SWEEP mode, remember that the DAC is updated\n");
  sendback(filedes,"                                at next sampling clock cycle, so the first sample in the shm must always\n");
  sendback(filedes,"                                be discarded; anyway, it's good to have, to check it's zero\n");

  }


//-------------------------------------------------------------------

void parse(char *buf, char *ans, size_t maxlen, int filedes, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int rw;

  trimstring(buf);
  upstring(buf);

  // is this a READ or WRITE operation?
  // note: cannot use strtok because it modifies the input string
  p=strchr(buf,'?');
  if(p!=NULL)
    {
    rw=SCPI_READ;
    p=strtok(buf,"?");
    }
  else
    {
    rw=SCPI_WRITE;
    p=strtok(buf," ");
    }

  // serve the right command
  if(strcmp(p,"*IDN")==0)
    parse_IDN(ans, maxlen);
  else if(strcmp(p,"*STB")==0)
    parse_STB(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"*RST")==0)
    parse_RST(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"DAC")==0)
    parse_DAC(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"DACCH")==0)
    parse_DACCH(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"ADC")==0)
    parse_READ_ADC(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"FSAMPL")==0)
    parse_FSAMPL(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"WAVEGEN")==0)
    parse_WAVEGEN(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"WAVEGEN:CH_CONFIG")==0)
    parse_WAVEGEN_CH_CONFIG(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"WAVEGEN:CH_ENABLE")==0)
    parse_WAVEGEN_CH_ENABLE(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if( (strcmp(p,"RECORD:TRIG")==0) || (strcmp(p,"RECORD:TRIGGER")==0) )
    parse_TRIG(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if( (strcmp(p,"RECORD:TRIG:SETUP")==0) || (strcmp(p,"RECORD:TRIGGER:SETUP")==0) )
    parse_TRIGSETUP(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"RECORD:SAMPLES")==0)
    {
    printSamples(filedes);
    *ans=0;
    }
  else if(strcmp(p,"HELP")==0)
    {
    printHelp(filedes);
    *ans=0;
    }
  else
    {
    snprintf(ans, maxlen, "%s: no such command\n", SCPI_ERRS);
    }
  }


//-------------------------------------------------------------------

void sendback(int filedes, char *s)
  {
  (void)write(filedes, s, strlen(s));
  }


//-------------------------------------------------------------------

int SCPI_read_from_client(int filedes, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char buffer[SCPI_MAXMSG+1];    // "+1" to add zero-terminator
  char answer[SCPI_MAXMSG+1];
  int  nbytes;
  
  nbytes = read(filedes, buffer, SCPI_MAXMSG);
  if(nbytes < 0)
    {
    // read error
    perror("read");
    exit(EXIT_FAILURE);
    }
  else if(nbytes == 0)
    {
    // end of file
    return -1;
    }
  else
    {
    // data read
    buffer[nbytes]=0;    // add string zero terminator
    //fprintf(stderr, "Incoming msg: '%s'\n", buffer);
    parse(buffer, answer, SCPI_MAXMSG, filedes, endp_ptr, rpmsg_ptr);
    sendback(filedes, answer);
    return 0;
    }
  }


//-------------------------------------------------------------------

int startSCPIserver(int *sock_ptr, fd_set *active_fd_set_ptr)
  {
  int opt = 1;
  struct sockaddr_in name;

  LPRINTF("Starting server\r\n");
  *sock_ptr = socket(PF_INET, SOCK_STREAM, 0);
  if(*sock_ptr < 0)
    {
    LPRINTF("socket() error\r\n");
    return SCPI_GEN_ERR;
    }

  if( setsockopt(*sock_ptr, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(int)) )
    {
    perror("setsockopt() error\r\n");
    return SCPI_GEN_ERR;
    }

  name.sin_family = AF_INET;
  name.sin_port = htons(SCPI_PORT);
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  if( bind( *sock_ptr, (struct sockaddr *) &name, sizeof(name)) < 0)
    {
    LPRINTF("bind() error\r\n");
    return SCPI_GEN_ERR;
    }

  if(listen(*sock_ptr, 1) < 0)
    {
    LPRINTF("listen() error\r\n");
    return SCPI_GEN_ERR;
    }
  
  // initialize the set of active sockets; 
  // see man 2 select for FD_functions documentation
  FD_ZERO(active_fd_set_ptr);
  FD_SET(*sock_ptr, active_fd_set_ptr);

  return SCPI_NO_ERR;
  }
