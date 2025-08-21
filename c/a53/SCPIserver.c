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
      {
      snprintf(ans, maxlen, "%s: STB READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
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
    // READ

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_DAC;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: DAC READBACK rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
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
  else
    {
    // WRITE

    // parse the values to write into the 4 DAC channels;
    // format is 16 bit 2's complement int, written in decimal
    for(nch=0; nch<4; nch++)
      {
      // next in line is the desired value for nex DAC channel
      p=strtok(NULL," ");
      if(p==NULL)
        break;
      errno=0;
      n=(int)strtol(p, NULL, 10);
      if(errno!=0 && n==0)
        {
        snprintf(ans, maxlen, "%s: invalid DAC value\n", SCPI_ERRS);
        break;
        }
      dac[nch]=n;
      }

    // now send the new values to R5
    if((p==NULL) || (nch!=4))
      {
      snprintf(ans, maxlen, "%s: missing DAC channel value\n", SCPI_ERRS);
      return;
      }
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
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing DAC channel\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid DAC channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: DAC channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }
    // next in line is the DAC value
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing DAC value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    dacvalue=(int)strtol(p, NULL, 10);
    if(errno!=0 && dacvalue==0)
      {
      snprintf(ans, maxlen, "%s: invalid DAC value\n", SCPI_ERRS);
      return;
      }
    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_WRITE_DACCH;
    rpmsg_ptr->data[0]=((u32)(nch)<<16)&0xFFFF0000 | ((u32)(dacvalue))&0x0000FFFF;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: DAC:CH WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s: DAC CH %d updated with value %d\n", SCPI_OKS, nch, dacvalue);
    }
  }


//-------------------------------------------------------------------

void parse_DACOFFS(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch,offs;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // READ: request DAC channel offset

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing DAC channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid DAC channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: DAC channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_DACOFFS;
    rpmsg_ptr->data[0] = (u32)(nch);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: DAC OFFS READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        snprintf(ans, maxlen, "%s: %d is the offset of DAC channel #%d\n", SCPI_OKS, gDAC_offs_cnt[nch-1], nch);
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: DAC OFFS READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: DAC OFFS READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE: set DAC oddset correction for a particular channel

    // parse the values of DAC channel offset;
    // format is 16 bit 2's complement int, written in decimal

    // next in line is the desired DAC channel
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing DAC channel\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid DAC channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: DAC channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }
    // next in line is the DAC offset value
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing OFFSET value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    offs=(int)strtol(p, NULL, 10);
    if(errno!=0 && offs==0)
      {
      snprintf(ans, maxlen, "%s: invalid DAC offset\n", SCPI_ERRS);
      return;
      }
    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_WRITE_DACOFFS;
    rpmsg_ptr->data[0]=(u32)(nch);
    rpmsg_ptr->data[1]=(u32)(offs);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: DAC:CH:OFFS WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s: DAC CH %d has offset %d\n", SCPI_OKS, nch, offs);
    }
  }


//-------------------------------------------------------------------

void parse_DAC_OUT_SELECT(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch,insel;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // READ: DAC output selector state (control loop/wave generator)

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing DAC channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid DAC channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: DAC channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_DACOUTSEL;
    rpmsg_ptr->data[0] = (u32)(nch);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: DAC OUT SEL READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        snprintf(ans, maxlen, "%s: %d\n", SCPI_OKS, gDAC_outputSelect[nch-1]+1);
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: DAC OUT SEL READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: DAC OUT SEL READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE DAC output selector state (control loop/wave generator)

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing DAC channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid DAC channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: DAC channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the input selector
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing input selector\n", SCPI_ERRS);
      return;
      }
    errno=0;
    insel=(int)strtol(p, NULL, 10);
    if(errno!=0 && insel==0)
      {
      snprintf(ans, maxlen, "%s: invalid input selector\n", SCPI_ERRS);
      return;
      }
    if((insel<1)||(insel>5))
      {
      snprintf(ans, maxlen, "%s: input selector out of range [1..5]\n", SCPI_ERRS);
      return;
      }

    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_WRITE_DACOUTSEL;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(insel);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: DAC OUT SEL WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s: DAC OUT driver updated\n", SCPI_OKS);
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
      {
      snprintf(ans, maxlen, "%s: ADC READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
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
  else
    {
    snprintf(ans, maxlen, "%s: write operation not supported\n", SCPI_ERRS);
    }
  }


//-------------------------------------------------------------------

void parse_ADCOFFS(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch,offs;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // READ: request ADC channel offset

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing ADC channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid ADC channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: ADC channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_ADCOFFS;
    rpmsg_ptr->data[0] = (u32)(nch);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: ADC OFFS READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        snprintf(ans, maxlen, "%s: %d is the offset of ADC channel #%d\n", SCPI_OKS, gADC_offs_cnt[nch-1], nch);
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: ADC OFFS READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: ADC OFFS READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE: set ADC offset correction for a particular channel

    // parse the values of ADC channel offset;
    // format is 16 bit 2's complement int, written in decimal

    // next in line is the desired ADC channel
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing ADC channel\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid ADC channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: ADC channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }
    // next in line is the ADC offset value
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing OFFSET value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    offs=(int)strtol(p, NULL, 10);
    if(errno!=0 && offs==0)
      {
      snprintf(ans, maxlen, "%s: invalid ADC offset\n", SCPI_ERRS);
      return;
      }
    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_WRITE_ADCOFFS;
    rpmsg_ptr->data[0]=(u32)(nch);
    rpmsg_ptr->data[1]=(u32)(offs);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: ADC:CH:OFFS WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s: ADC CH %d has offset %d\n", SCPI_OKS, nch, offs);
    }
  }


//-------------------------------------------------------------------

void parse_ADCGAIN(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch;
  int numbytes, rpmsglen, status;
  float g;

  
  if(rw==SCPI_READ)
    {
    // READ: request ADC channel offset

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing ADC channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid ADC channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: ADC channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_ADCGAIN;
    rpmsg_ptr->data[0] = (u32)(nch);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: ADC GAIN READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        snprintf(ans, maxlen, "%s: %f is the gain of ADC channel #%d\n", SCPI_OKS, gADC_gain[nch-1], nch);
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: ADC GAIN READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: ADC GAIN READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE: set ADC gain correction for a particular channel

    // parse the values of ADC channel gain correction (float)

    // next in line is the desired ADC channel
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing ADC channel\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid ADC channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: ADC channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }
    // next in line is the ADC gain value (float)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing GAIN value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    g=strtof(p, NULL);
    // error check with float does not work
    // if(errno!=0 && errno!=EIO && g==0.0F)
    //   {
    //   snprintf(ans, maxlen, "%s: invalid ADC gain\n", SCPI_ERRS);
    //   return;
    //   }
    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_WRITE_ADCGAIN;
    rpmsg_ptr->data[0] = (u32)(nch);
    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[1]), &g, sizeof(u32));
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: ADC:CH:GAIN WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s: ADC CH %d has gain %f\n", SCPI_OKS, nch, g);
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
    // READ
    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_FSAMPL;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: FSAMPL READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
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
  else
    {
    // WRITE
    // parse the desired sampling frequency in Hz
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing sampling frequency\n", SCPI_ERRS);
      return;
      }
    errno=0;
    fs=(u32)strtol(p, NULL, 10);
    if(errno!=0 && fs==0)
      {
      snprintf(ans, maxlen, "%s: invalid sampling frequency\n", SCPI_ERRS);
      return;
      }
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
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", SCPI_ERRS);
      return;
      }
    if(strcmp(p,"ON")==0)
      onoff=1;
    else if(strcmp(p,"OFF")==0)
      onoff=0;
    else
      onoff=-1;
    
    if(onoff<0)
      {
      snprintf(ans, maxlen, "%s: use ON/OFF with WAVEGEN command\n", SCPI_ERRS);
      return;
      }
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
    errno=0;
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

        snprintf(ans, maxlen, "%s: %s %.8g %.8g %.8g %.8g %.8g\n",
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
    errno=0;
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
    errno=0;
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
      errno=0;
      x=strtof(p, NULL);
      // 0.0 is an acceptable value for the offset;
      // error check with float does not work
      // if(errno!=0 && errno!=EIO && x==0.0F)
      //   {
      //   snprintf(ans, maxlen, "%s: invalid offset (%.8g; errno=%d)\n", SCPI_ERRS,x,errno);
      //   return;
      //   }
      }
    else
      x=0.0F;

    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[3]), &x, sizeof(u32));
    //LPRINTF(">>%.8g<<\n",*((float*)&(rpmsg_ptr->data[3])));


    // next in line is f1 (for SINE and SWEEP waveforms, not for DC)
    if(wtype!=WGEN_CH_TYPE_DC)
      {
      p=strtok(NULL," ");
      if(p==NULL)
        {
        snprintf(ans, maxlen, "%s: missing frequency\n", SCPI_ERRS);
        return;
        }
      errno=0;
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
      errno=0;
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
      errno=0;
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

void parse_WAVEGEN_CH_STATE(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
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
    errno=0;
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
    rpmsg_ptr->command = RPMSGCMD_READ_WGEN_CH_STATE;
    rpmsg_ptr->data[0] = (u32)(nch);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: WAVEGEN CHAN STATE READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:

        switch(gWavegenChanConfig[nch-1].state)
          {
          case WGEN_CH_STATE_OFF:
            strcpy(enblstr,"OFF");
            break;
          case WGEN_CH_STATE_ON:
            strcpy(enblstr,"ON");
            break;
          case WGEN_CH_STATE_SINGLE:
            strcpy(enblstr,"SINGLE");
            break;
          }

        snprintf(ans, maxlen, "%s: %s\n", SCPI_OKS, enblstr);
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: WAVEGEN CH STATE READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: WAVEGEN CH STATE READ error\n", SCPI_ERRS);
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
    errno=0;
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
      enbl=WGEN_CH_STATE_OFF;
    else if(strcmp(p,"ON")==0)
      enbl=WGEN_CH_STATE_ON;
    else if(strcmp(p,"SINGLE")==0)
      enbl=WGEN_CH_STATE_SINGLE;
    else
      enbl=-1;
    
    if(enbl<0)
      {
      snprintf(ans, maxlen, "%s: use ON/OFF/SINGLE for WAVEGEN channel state\n", SCPI_ERRS);
      return;
      }


    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_WRITE_WGEN_CH_STATE;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(enbl);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: WAVEGEN CH STATE WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s: WAVEGEN chan %d is now %s\n", SCPI_OKS, nch, p);
    }

  }


//-------------------------------------------------------------------

void parse_CTRLLOOP(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
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
    // turn control loop ON or OFF

    // next in line is ON or OFF
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", SCPI_ERRS);
      return;
      }
    if(strcmp(p,"ON")==0)
      onoff=1;
    else if(strcmp(p,"OFF")==0)
      onoff=0;
    else
      onoff=-1;
    
    if(onoff<0)
      {
      snprintf(ans, maxlen, "%s: use ON/OFF with CTRLLOOP command\n", SCPI_ERRS);
      return;
      }
    // send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_CTRLLOOP_ONOFF;
    rpmsg_ptr->data[0]=(u32)onoff;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: CTRLLOOP WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s: control loop switched %s\n", SCPI_OKS, onoff==1? "ON" : "OFF");
    }
  }


//-------------------------------------------------------------------

void do_FilterReset(int filtertype, char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch, instance;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    snprintf(ans, maxlen, "%s: read operation not supported\n", SCPI_ERRS);
    }
  else
    {
    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = (filtertype==FILTERTYPE_PID) ? RPMSGCMD_RESET_PID : RPMSGCMD_RESET_IIR;
    rpmsg_ptr->data[0]=(u32)nch;
    rpmsg_ptr->data[1]=(u32)instance;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: FILTRESET WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s\n", SCPI_OKS);
    }

  }


//-------------------------------------------------------------------

void parse_PIDRESET(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  do_FilterReset(FILTERTYPE_PID,ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  }


//-------------------------------------------------------------------

void parse_IIRRESET(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  do_FilterReset(FILTERTYPE_IIR,ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  }


//-------------------------------------------------------------------

void parse_IIRCOEFF(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch, instance, i;
  float x;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // READ: request IIR coefficients

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_IIR_COEFF;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(instance);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: IIR COEFF READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        snprintf(ans, maxlen, "%s: %.8g %.8g %.8g %.8g %.8g\n",
                    SCPI_OKS,
                    gCtrlLoopChanConfig[nch-1].IIR[instance-1].a[0],
                    gCtrlLoopChanConfig[nch-1].IIR[instance-1].a[1],
                    gCtrlLoopChanConfig[nch-1].IIR[instance-1].a[2],
                    gCtrlLoopChanConfig[nch-1].IIR[instance-1].b[0],
                    gCtrlLoopChanConfig[nch-1].IIR[instance-1].b[1]
                );
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: IIR COEFF READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: IIR COEFF READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE: set IIR COEFF

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // start filling the rpmsg with the integer parameters
    rpmsg_ptr->command = RPMSGCMD_WRITE_IIR_COEFF;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(instance);

    // now parse floating point values;

    // first parse A0 to A2
    for(i=0; i<3; i++)
      {
      p=strtok(NULL," ");
      if(p==NULL)
        {
        snprintf(ans, maxlen, "%s: missing A%d\n", SCPI_ERRS, i);
        return;
        }
      errno=0;
      x=strtof(p, NULL);
      // error check with float does not work
      // if(errno!=0 && errno!=EIO && x==0.0F)
      //   {
      //   snprintf(ans, maxlen, "%s: invalid A%d\n", SCPI_ERRS, i);
      //   return;
      //   }
      // write floating point values directly as float (32 bit)
      memcpy(&(rpmsg_ptr->data[2+i]), &x, sizeof(u32));
      }

    // then parse B0 and B1
    for(i=0; i<2; i++)
      {
      p=strtok(NULL," ");
      if(p==NULL)
        {
        snprintf(ans, maxlen, "%s: missing B%d\n", SCPI_ERRS, i);
        return;
        }
      errno=0;
      x=strtof(p, NULL);
      // error check with float does not work
      // if(errno!=0 && errno!=EIO && x==0.0F)
      //   {
      //   snprintf(ans, maxlen, "%s: invalid B%d\n", SCPI_ERRS, i);
      //   return;
      //   }
      // write floating point values directly as float (32 bit)
      memcpy(&(rpmsg_ptr->data[5+i]), &x, sizeof(u32));
      }

    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: IIR COEFF WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s\n", SCPI_OKS);
    }

  }


//-------------------------------------------------------------------

void parse_MATRIXROW(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int matrixindex, matrixrow, i;
  float x, *xp;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // READ: request MIMO matrix row

    // next in line is the matrix name (A to F)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing matrix name\n", SCPI_ERRS);
      return;
      }
    if(strlen(p)!=1)
      {
      snprintf(ans, maxlen, "%s: use A to F as matrix name\n", SCPI_ERRS);
      return;
      }
    matrixindex=p[0]-'A';
    if((matrixindex<0) || (matrixindex>5))
      {
      snprintf(ans, maxlen, "%s: use A to F as matrix name\n", SCPI_ERRS);
      return;
      }

    // next in line is matrix row (1 to 4)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing matrix row number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    matrixrow=(int)strtol(p, NULL, 10);
    if(errno!=0 && matrixrow==0)
      {
      snprintf(ans, maxlen, "%s: invalid matrix row number\n", SCPI_ERRS);
      return;
      }
    if((matrixrow<1)||(matrixrow>4))
      {
      snprintf(ans, maxlen, "%s: matrix row number out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_MATRIX_ROW;
    rpmsg_ptr->data[0] = (u32)(matrixindex);
    rpmsg_ptr->data[1] = (u32)(matrixrow);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: MATRIX ROW READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        switch(matrixindex)
          {
          case 0:
            xp=gCtrlLoopChanConfig[matrixrow-1].input_MISO_A;
            break;
          case 1:
            xp=gCtrlLoopChanConfig[matrixrow-1].input_MISO_B;
            break;
          case 2:
            xp=gCtrlLoopChanConfig[matrixrow-1].input_MISO_C;
            break;
          case 3:
            xp=gCtrlLoopChanConfig[matrixrow-1].input_MISO_D;
            break;
          case 4:
            xp=gCtrlLoopChanConfig[matrixrow-1].output_MISO_E;
            break;
          case 5:
            xp=gCtrlLoopChanConfig[matrixrow-1].output_MISO_F;
            break;
          }
        snprintf(ans, maxlen, "%s: %.8g %.8g %.8g %.8g %.8g\n",
                    SCPI_OKS,
                    *xp, *(xp+1), *(xp+2), *(xp+3), *(xp+4)
                );
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: MATRIX ROW READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: MATRIX ROW READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE: write MIMO matrix row
    
    // next in line is the matrix name (A to F)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing matrix name\n", SCPI_ERRS);
      return;
      }
    if(strlen(p)!=1)
      {
      snprintf(ans, maxlen, "%s: use A to F as matrix name\n", SCPI_ERRS);
      return;
      }
    matrixindex=p[0]-'A';
    if((matrixindex<0) || (matrixindex>5))
      {
      snprintf(ans, maxlen, "%s: use A to F as matrix name\n", SCPI_ERRS);
      return;
      }

    // next in line is matrix row (1 to 4)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing matrix row number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    matrixrow=(int)strtol(p, NULL, 10);
    if(errno!=0 && matrixrow==0)
      {
      snprintf(ans, maxlen, "%s: invalid matrix row number\n", SCPI_ERRS);
      return;
      }
    if((matrixrow<1)||(matrixrow>4))
      {
      snprintf(ans, maxlen, "%s: matrix row number out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // start filling the rpmsg with the integer parameters
    rpmsg_ptr->command = RPMSGCMD_WRITE_MATRIX_ROW;
    rpmsg_ptr->data[0] = (u32)(matrixindex);
    rpmsg_ptr->data[1] = (u32)(matrixrow);

    // now parse floating point coefficients

    for(i=0; i<5; i++)
      {
      p=strtok(NULL," ");
      if(p==NULL)
        {
        snprintf(ans, maxlen, "%s: missing %c(%d,%d)\n", SCPI_ERRS, 'A'+matrixindex, matrixrow, i);
        return;
        }
      errno=0;
      x=strtof(p, NULL);
      // error check with float does not work
      // if(errno!=0 && errno!=EIO && x==0.0F)
      //   {
      //   snprintf(ans, maxlen, "%s: invalid %c(%d,%d)\n", SCPI_ERRS, 'A'+matrixindex, matrixrow, i);
      //   return;
      //   }
      // write floating point values directly as float (32 bit)
      memcpy(&(rpmsg_ptr->data[2+i]), &x, sizeof(u32));
      }


    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: MATRIX ROW WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s\n", SCPI_OKS);
    }

  }


//-------------------------------------------------------------------

void parse_PIDGAINS(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch, instance;
  float x;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // READ: request PID gains

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_PID_GAINS;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(instance);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: PID GAINS READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        snprintf(ans, maxlen, "%s: %.8g %.8g %.8g %.8g %.8g\n",
                    SCPI_OKS,
                    gCtrlLoopChanConfig[nch-1].PID[instance-1].Gp,
                    gCtrlLoopChanConfig[nch-1].PID[instance-1].Gi,
                    gCtrlLoopChanConfig[nch-1].PID[instance-1].G1d,
                    gCtrlLoopChanConfig[nch-1].PID[instance-1].G2d,
                    gCtrlLoopChanConfig[nch-1].PID[instance-1].G_aiw
                );
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: PID GAINS READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: PID GAINS READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE: set PID gains

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // start filling the rpmsg with the integer parameters
    rpmsg_ptr->command = RPMSGCMD_WRITE_PID_GAINS;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(instance);

    // now parse floating point values; 

    // next in line is Gp
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing Gp\n", SCPI_ERRS);
      return;
      }
    errno=0;
    x=strtof(p, NULL);
    // error check with float does not work
    // if(errno!=0 && errno!=EIO && x==0.0F)
    //   {
    //   snprintf(ans, maxlen, "%s: invalid Gp\n", SCPI_ERRS);
    //   return;
    //   }
    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[2]), &x, sizeof(u32));

    // next in line is Gi
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing Gi\n", SCPI_ERRS);
      return;
      }
    errno=0;
    x=strtof(p, NULL);
    // error check with float does not work
    // if(errno!=0 && errno!=EIO && x==0.0F)
    //   {
    //   snprintf(ans, maxlen, "%s: invalid Gi\n", SCPI_ERRS);
    //   return;
    //   }
    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[3]), &x, sizeof(u32));

    // next in line is G1D
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing G1D\n", SCPI_ERRS);
      return;
      }
    errno=0;
    x=strtof(p, NULL);
    // error check with float does not work
    // if(errno!=0 && errno!=EIO && x==0.0F)
    //   {
    //   snprintf(ans, maxlen, "%s: invalid G1D\n", SCPI_ERRS);
    //   return;
    //   }
    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[4]), &x, sizeof(u32));

    // next in line is G2D
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing G2D\n", SCPI_ERRS);
      return;
      }
    errno=0;
    x=strtof(p, NULL);
    // error check with float does not work
    // if(errno!=0 && errno!=EIO && x==0.0F)
    //   {
    //   snprintf(ans, maxlen, "%s: invalid G2D\n", SCPI_ERRS);
    //   return;
    //   }
    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[5]), &x, sizeof(u32));

    // next in line is G_AIW (anti integral windup)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing G_AIW\n", SCPI_ERRS);
      return;
      }
    errno=0;
    x=strtof(p, NULL);
    // error check with float does not work
    // if(errno!=0 && errno!=EIO && x==0.0F)
    //   {
    //   snprintf(ans, maxlen, "%s: invalid G_AIW\n", SCPI_ERRS);
    //   return;
    //   }
    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[6]), &x, sizeof(u32));


    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: PID GAINS WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s\n", SCPI_OKS);
    }

  }


//-------------------------------------------------------------------

void parse_PIDTHRESH(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch, instance;
  float x;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // READ: request PID thresholds

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_PID_THR;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(instance);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: PID THR READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        snprintf(ans, maxlen, "%s: %.8g %.8g\n",
                    SCPI_OKS,
                    gCtrlLoopChanConfig[nch-1].PID[instance-1].in_thr,
                    gCtrlLoopChanConfig[nch-1].PID[instance-1].out_sat
                );
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: PID THR READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: PID THR READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE: set PID thresholds

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // start filling the rpmsg with the integer parameters
    rpmsg_ptr->command = RPMSGCMD_WRITE_PID_THR;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(instance);

    // now parse floating point values; 

    // next in line is input deadband
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing input dead band\n", SCPI_ERRS);
      return;
      }
    errno=0;
    x=strtof(p, NULL);
    // error check with float does not work
    // if(errno!=0 && errno!=EIO && x==0.0F)
    //   {
    //   snprintf(ans, maxlen, "%s: invalid input dead band\n", SCPI_ERRS);
    //   return;
    //   }
    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[2]), &x, sizeof(u32));

    // next in line is output saturation
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing output saturation\n", SCPI_ERRS);
      return;
      }
    errno=0;
    x=strtof(p, NULL);
    // error check with float does not work
    // if(errno!=0 && errno!=EIO && x==0.0F)
    //   {
    //   snprintf(ans, maxlen, "%s: invalid output saturation value\n", SCPI_ERRS);
    //   return;
    //   }
    // write floating point values directly as float (32 bit)
    memcpy(&(rpmsg_ptr->data[3]), &x, sizeof(u32));


    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: PID THR WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s\n", SCPI_OKS);
    }

  }


//-------------------------------------------------------------------

void parse_PID_PVDERIV(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch, instance, enbl;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // READ

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_PID_PV_DERIV;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(instance);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: PID PV_DERIV READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        snprintf(ans, maxlen, "%s: %s is the state of derivative on process variable\n",
                    SCPI_OKS,
                    gCtrlLoopChanConfig[nch-1].PID[instance-1].deriv_on_PV ? "ON" : "OFF"
                );
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: PID PV_DERIV READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: PID PV_DERIV READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE: enable/disable derivative on process variable

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // next in line is the state (ON/OFF)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", SCPI_ERRS);
      return;
      }
    if(strcmp(p,"OFF")==0)
      enbl=0;
    else if(strcmp(p,"ON")==0)
      enbl=1;
    else
      enbl=-1;
    
    if(enbl<0)
      {
      snprintf(ans, maxlen, "%s: use ON/OFF for derivative on process variable\n", SCPI_ERRS);
      return;
      }

    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_WRITE_PID_PV_DERIV;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(instance);
    rpmsg_ptr->data[2] = (u32)(enbl);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: PID PV_DERIV WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s\n", SCPI_OKS);
    }

  }


//-------------------------------------------------------------------

void parse_PID_INVCMD(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch, instance, enbl;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // READ

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_PID_INVCMD;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(instance);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: PID INV_CMD READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        snprintf(ans, maxlen, "%s: %s is the state of command inversion\n",
                    SCPI_OKS,
                    gCtrlLoopChanConfig[nch-1].PID[instance-1].invert_cmd ? "ON" : "OFF"
                );
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: PID INV_CMD READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: PID INV_CMD READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE: enable/disable command inversion

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // next in line is the state (ON/OFF)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", SCPI_ERRS);
      return;
      }
    if(strcmp(p,"OFF")==0)
      enbl=0;
    else if(strcmp(p,"ON")==0)
      enbl=1;
    else
      enbl=-1;
    
    if(enbl<0)
      {
      snprintf(ans, maxlen, "%s: use ON/OFF for command inversion\n", SCPI_ERRS);
      return;
      }

    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_WRITE_PID_INVCMD;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(instance);
    rpmsg_ptr->data[2] = (u32)(enbl);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: PID INV_CMD WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s\n", SCPI_OKS);
    }

  }


//-------------------------------------------------------------------

void parse_PID_INVMEAS(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch, instance, enbl;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // READ

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_PID_INVMEAS;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(instance);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: PID INV_MEAS READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        snprintf(ans, maxlen, "%s: %s is the state of measurement input inversion\n",
                    SCPI_OKS,
                    gCtrlLoopChanConfig[nch-1].PID[instance-1].invert_meas ? "ON" : "OFF"
                );
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: PID INV_MEAS READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: PID INV_MEAS READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE: enable/disable measurement input  inversion

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid CTRLLOOP channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the instance (1 or 2)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing instance value\n", SCPI_ERRS);
      return;
      }
    errno=0;
    instance=(int)strtol(p, NULL, 10);
    if(errno!=0 && instance==0)
      {
      snprintf(ans, maxlen, "%s: invalid instance value\n", SCPI_ERRS);
      return;
      }
    if((instance<1)||(instance>2))
      {
      snprintf(ans, maxlen, "%s: instance value out of range [1..2]\n", SCPI_ERRS);
      return;
      }

    // next in line is the state (ON/OFF)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", SCPI_ERRS);
      return;
      }
    if(strcmp(p,"OFF")==0)
      enbl=0;
    else if(strcmp(p,"ON")==0)
      enbl=1;
    else
      enbl=-1;
    
    if(enbl<0)
      {
      snprintf(ans, maxlen, "%s: use ON/OFF for measurement input inversion\n", SCPI_ERRS);
      return;
      }

    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_WRITE_PID_INVMEAS;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(instance);
    rpmsg_ptr->data[2] = (u32)(enbl);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: PID INV_MEAS WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s\n", SCPI_OKS);
    }

  }


//-------------------------------------------------------------------

void parse_CTRLLOOP_CH_STATE(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch,enbl;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // READ: request control loop channel ON/OFF state

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing control loop channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid control loop channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: control loop channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_CTRLLOOP_CH_STATE;
    rpmsg_ptr->data[0] = (u32)(nch);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP CHAN STATE READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        snprintf(ans, maxlen, "%s: %s\n", SCPI_OKS, (gCtrlLoopChanConfig[nch-1].state == CTRLLOOP_CH_ENABLED) ? "ON" : "OFF");
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: CTRLLOOP CH ENABLE READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: CTRLLOOP CH ENABLE READ error\n", SCPI_ERRS);
        break;
      }

    }
  else
    {
    // WRITE: enable/disable one control loop channel

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing control loop channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid control loop channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: control loop channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the state (ON/OFF)
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing ON/OFF option\n", SCPI_ERRS);
      return;
      }
    if(strcmp(p,"OFF")==0)
      enbl=CTRLLOOP_CH_DISABLED;
    else if(strcmp(p,"ON")==0)
      enbl=CTRLLOOP_CH_ENABLED;
    else
      enbl=-1;
    
    if(enbl<0)
      {
      snprintf(ans, maxlen, "%s: use ON/OFF for control loop channel state\n", SCPI_ERRS);
      return;
      }


    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_WRITE_CTRLLOOP_CH_STATE;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(enbl);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: CTRLLOOP CH STATE WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s: control loop chan %d is now %s\n", SCPI_OKS, nch, p);
    }

  }


//-------------------------------------------------------------------

void parse_CTRLLOOP_CH_INSEL(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
  int nch,insel;
  int numbytes, rpmsglen, status;

  
  if(rw==SCPI_READ)
    {
    // READ: request control loop channel input selector

    // next in line is the channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing control loop channel number\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid control loop channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: control loop channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // now send rpmsg to R5 with the request

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_CTRLLOOP_CH_INSEL;
    rpmsg_ptr->data[0] = (u32)(nch);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: CTRLLOOP CH IN_SEL READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
    // now wait for answer
    status=WaitForRpmsg();
    switch(status)
      {
      case RPMSG_ANSWER_VALID:
        snprintf(ans, maxlen, "%s: %d\n", SCPI_OKS, gCtrlLoopChanConfig[nch-1].inputSelect+1);
        break;
      case RPMSG_ANSWER_TIMEOUT:
        snprintf(ans, maxlen, "%s: CTRLLOOP CH IN_SEL READ timed out\n", SCPI_ERRS);
        break;
      case RPMSG_ANSWER_ERR:
        snprintf(ans, maxlen, "%s: CTRLLOOP CH IN_SEL READ error\n", SCPI_ERRS);
        break;
      }
    }
  else
    {
    // WRITE: set control loop channel input selector

    // next in line is channel number
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing control loop channel\n", SCPI_ERRS);
      return;
      }
    errno=0;
    nch=(int)strtol(p, NULL, 10);
    if(errno!=0 && nch==0)
      {
      snprintf(ans, maxlen, "%s: invalid control loop channel number\n", SCPI_ERRS);
      return;
      }
    if((nch<1)||(nch>4))
      {
      snprintf(ans, maxlen, "%s: control loop channel out of range [1..4]\n", SCPI_ERRS);
      return;
      }

    // next in line is the input selector
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing input selector\n", SCPI_ERRS);
      return;
      }
    errno=0;
    insel=(int)strtol(p, NULL, 10);
    if(errno!=0 && insel==0)
      {
      snprintf(ans, maxlen, "%s: invalid input selector\n", SCPI_ERRS);
      return;
      }
    if((insel<1)||(insel>5))
      {
      snprintf(ans, maxlen, "%s: input selector out of range [1..5]\n", SCPI_ERRS);
      return;
      }

    // everything is ready: send rpmsg to R5
    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_WRITE_CTRLLOOP_CH_INSEL;
    rpmsg_ptr->data[0] = (u32)(nch);
    rpmsg_ptr->data[1] = (u32)(insel);
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      snprintf(ans, maxlen, "%s: CTRLLOOP:CH:IN_SEL WRITE rpmsg_send() failed\n", SCPI_ERRS);
    else
      snprintf(ans, maxlen, "%s\n", SCPI_OKS);
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
    // READ

    // remove stale rpmsgs from queue
    FlushRpmsg();

    rpmsglen=sizeof(R5_RPMSG_TYPE);
    rpmsg_ptr->command = RPMSGCMD_READ_TRIG;
    numbytes= rpmsg_send(endp_ptr, rpmsg_ptr, rpmsglen);
    if(numbytes<rpmsglen)
      {
      snprintf(ans, maxlen, "%s: TRIG READ rpmsg_send() failed\n", SCPI_ERRS);
      return;
      }
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
  else
    {
    // WRITE

    // ARM/DISARM trigger

    // next in line is ARM/FORCE/OFF
    p=strtok(NULL," ");
    if(p==NULL)
      {
      snprintf(ans, maxlen, "%s: missing ARM/FORCE/OFF option\n", SCPI_ERRS);
      return;
      }
    if(strcmp(p,"ARM")==0)
      trigstate=RECORDER_ARMED;
    else if(strcmp(p,"FORCE")==0)
      trigstate=RECORDER_FORCE;
    else if(strcmp(p,"OFF")==0)
      trigstate=RECORDER_IDLE;
    else
      trigstate=-1;
    
    if(trigstate<0)
      {
      snprintf(ans, maxlen, "%s: use ARM/FORCE/OFF with RECORD:TRIG command\n", SCPI_ERRS);
      return;
      }
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
          snprintf(ans, maxlen, "%s: %d %s %s %.8g\n", SCPI_OKS, 
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
    errno=0;
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
      errno=0;
      lvl=strtof(p, NULL);
      // error check with float does not work
      //if(errno!=0 && errno!=EIO && lvl==0.0F)
      //  {
      //  snprintf(ans, maxlen, "%s: invalid TRIG level\n", SCPI_ERRS);
      //  return;
      //  }
  
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
  sendback(filedes,"DAC:CH <nch> <val>            : write value <val> into DAC channel <nch>;\n");
  sendback(filedes,"                                <nch> is in range [1..4];\n");
  sendback(filedes,"                                <val> is a 16-bit 2's complement integer\n");
  sendback(filedes,"                                in decimal notation\n");
  sendback(filedes,"DAC:CH:OFFSet <nch> <val>     : set/get the offset of DAC channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                <value> is in counts, in range [-32768,+32768] for uniformity\n");
  sendback(filedes,"                                with the ADC case\n");
  sendback(filedes,"CTRLLOOP:CH:OUT_SELect <nch> <src> :\n");
  sendback(filedes,"                                sets the driver of DAC channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                <src> = 5 selects the output of the E,F MISO matrix;\n");
  sendback(filedes,"                                <src> = 1..4 selects the corresponding waveform generator channel\n");
  sendback(filedes,"CTRLLOOP:CH:OUT_SELect? <nch>  : retrieves the driver of the DAC channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                answer is {WAVEGEN|CTRLLOOP}\n");
  sendback(filedes,"ADC?                          : read the values of the 4 ADC channels; note that\n");
  sendback(filedes,"                                the values are updated at every cycle\n");
  sendback(filedes,"                                of the sampling clock\n");
  sendback(filedes,"ADC:CH:OFFSet <nch> <val>     : set/get the offset of ADC channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                <value> is in counts, in range [-32768,+32768];\n");
  sendback(filedes,"                                note that +32768 is a valid offset and it can be used to shift\n");
  sendback(filedes,"                                the ADC range from [-32768,+32767] to [0,65535], which can be\n");
  sendback(filedes,"                                useful in some cases where normalization is involved\n");
  sendback(filedes,"                                (e.g. BPM readout)\n");
  sendback(filedes,"ADC:CH:Gain <nch> <val>       : set the gain of ADC channel <nch> (in range [1..4]);\n");
  sendback(filedes,"ADC:CH:Gain? <nch>            : get the gain of ADC channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                <value> is in floating point\n");
  sendback(filedes,"FSAMPL <val>                  : request to change sampling frequency to <val> Hz;\n");
  sendback(filedes,"                                it will be approximated to the closest possible frequency;\n");
  sendback(filedes,"                                use FSAMPL? to retrieve the actual value set\n");
  sendback(filedes,"FSAMPL?                       : retrieve sampling frequency (1Hz precision)\n");
  sendback(filedes,"WAVEGEN {ON|OFF}              : global ON/OFF of waveform generator mode\n");
  sendback(filedes,"                                each channel must be individually enabled/disabled by\n");
  sendback(filedes,"                                the command WAVEGEN:CH:ENABLE\n");
  sendback(filedes,"WAVEGEN:CH:CONFIG <nch> {DC|SINE|SWEEP} <ampl> <offs> <freq1> <freq2> <sweeptime>\n");
  sendback(filedes,"                              : configures channel <chan> of the waveform generator;\n");
  sendback(filedes,"                                notes:\n");
  sendback(filedes,"                                - <nch> is in range [1..4];\n");
  sendback(filedes,"                                - <ampl> and <offs> are fraction of the fullscale, so\n");
  sendback(filedes,"                                  <ampl> is in (0,1) and <offs> is in (-1,1) range;\n");
  sendback(filedes,"                                - <freq1> and <freq2> are in Hz;\n");
  sendback(filedes,"                                - <sweeptime> is in seconds\n");
  sendback(filedes,"                                - DC takes only the argument <ampl>;\n");
  sendback(filedes,"                                - SINE takes only the arguments <ampl>, <offs> and <freq1>;\n");
  sendback(filedes,"WAVEGEN:CH:CONFIG? <nch>      : queries the configuration of waveform generator \n");
  sendback(filedes,"                                channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                the answer will be in the form:\n");
  sendback(filedes,"                                {DC|SINE|SWEEP} <ampl> <offs> <freq1> <freq2> <sweeptime>\n");
  sendback(filedes,"WAVEGEN:CH:STATE <nch> {OFF|ON|SINGLE}\n");
  sendback(filedes,"                              : enables/disables channel <nch> of the waveform generator;\n");
  sendback(filedes,"                                SINGLE can be used only in association with SWEEP;\n");
  sendback(filedes,"WAVEGEN:CH:STATE? <nch>       : retrieves on/off state of channel <nch> of the waveform generator\n");
  sendback(filedes,"CTRLLOOP {ON|OFF}             : global ON/OFF of control loop;\n");
  sendback(filedes,"                                each channel must be individually enabled/disabled by\n");
  sendback(filedes,"                                the command CTRLLOOP:CH:ENABLE\n");
  sendback(filedes,"CTRLLOOP:MATRIXROW {A|B|C|D|E|F} <rownum> <val0> <val1> <val2> <val3> <val4>\n");
  sendback(filedes,"                              : write a row of one of the 6 MIMO matrices (named A to F);\n");
  sendback(filedes,"                                <rownum> is in range [1..4]; values are floating point\n");
  sendback(filedes,"CTRLLOOP:MATRIXROW {A|B|C|D|E|F} <rownum> ?\n");
  sendback(filedes,"                              : retrieves a row of one of the 6 MIMO matrices (named A to F);\n");
  sendback(filedes,"                                <rownum> is in range [1..4]\n");
  sendback(filedes,"CTRLLOOP:CH:PID:RESET <nch> <instance>\n");
  sendback(filedes,"                              : reset memory of instance <instance> (in range [1..2]) of PID\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4])\n");
  sendback(filedes,"CTRLLOOP:CH:IIR:RESET <nch> <instance>\n");
  sendback(filedes,"                              : reset memory of instance <instance> (in range [1..2]) of IIR\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4])\n");
  sendback(filedes,"CTRLLOOP:CH:IIR:COEFF <nch> <instance> <A0> <A1> <A2> <B0> <B2>\n");
  sendback(filedes,"                              : set coefficients of instance <instance> (in range [1..2]) of IIR\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                coefficients are floating point values;\n");
  sendback(filedes,"CTRLLOOP:CH:IIR:COEFF? <nch> <instance>\n");
  sendback(filedes,"                              : retrieve coefficients of instance <instance> (in range [1..2]) of IIR\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4])\n");
  sendback(filedes,"CTRLLOOP:CH:STATE <nch> {ON|OFF}\n");
  sendback(filedes,"                              : enables/disables channel <nch> (in range [1..4]) of the control loop\n");
  sendback(filedes,"CTRLLOOP:CH:STATE? <nch>      : retrieves on/off state of control loop channel <nch> (in range [1..4])\n");
  sendback(filedes,"CTRLLOOP:CH:IN_SELect <nch> <src>\n");
  sendback(filedes,"                              : selects input to control loop channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                <src> = 5 selects the output of the C,D MISO matrix;\n");
  sendback(filedes,"                                <src> = 1..4 selects the corresponding waveform generator channel\n");
  sendback(filedes,"CTRLLOOP:CH:IN_SELect? <nch>  : retrieves the input to control loop channel <nch> (in range [1..4]);\n");
  sendback(filedes,"CTRLLOOP:CH:PID:Gains <nch> <instance> <Gp> <Gi> <G1d> <G2d> <G_AIW>\n");
  sendback(filedes,"                              : set gains of instance <instance> (in range [1..2]) of PID\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4]).\n");
  sendback(filedes,"                                Gains are floating point values;\n");
  sendback(filedes,"                                <G_AIW> is the anti integral windup gain (usually= 1.0)\n");
  sendback(filedes,"CTRLLOOP:CH:PID:Gains? <nch> <instance>\n");
  sendback(filedes,"                              : retrieve gains of instance <instance> (in range [1..2]) of PID\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4])\n");
  sendback(filedes,"CTRLLOOP:CH:PID:THResholds <nch> <instance> <IN_dead_band> <OUT_saturation>\n");
  sendback(filedes,"                              : set input deadband and output saturation thresholds\n");
  sendback(filedes,"                                of instance <instance> (in range [1..2]) of PID\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                values are floating point with full scale +/-1.0\n");
  sendback(filedes,"CTRLLOOP:CH:PID:THResholds? <nch> <instance>\n");
  sendback(filedes,"                              : query input deadband and output saturation thresholds\n");
  sendback(filedes,"                                of instance <instance> (in range [1..2]) of PID\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4]);\n");
  sendback(filedes,"CTRLLOOP:CH:PID:PVDERIV <nch> <instance> {ON|OFF}\n");
  sendback(filedes,"                              : enables/disable derivative on process variable\n");
  sendback(filedes,"                                for instance <instance> (in range [1..2]) of PID\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4]);\n");
  sendback(filedes,"CTRLLOOP:CH:PID:PVDERIV? <nch> <instance>\n");
  sendback(filedes,"                              : queries whether the derivative is taken on process variable\n");
  sendback(filedes,"                                for instance <instance> (in range [1..2]) of PID\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                answer is {ON|OFF}\n");
  sendback(filedes,"CTRLLOOP:CH:PID:INVERTCMD <nch> <instance> {ON|OFF}\n");
  sendback(filedes,"                              : enables/disable command inversion\n");
  sendback(filedes,"                                for instance <instance> (in range [1..2]) of PID\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4]);\n");
  sendback(filedes,"CTRLLOOP:CH:PID:INVERTCMD? <nch> <instance>\n");
  sendback(filedes,"                              : queries whether the command is inverted\n");
  sendback(filedes,"                                in instance <instance> (in range [1..2]) of PID\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                answer is {ON|OFF}\n");
  sendback(filedes,"CTRLLOOP:CH:PID:INVERTMEAS <nch> <instance> {ON|OFF}\n");
  sendback(filedes,"                              : enables/disable measurement inversion\n");
  sendback(filedes,"                                for instance <instance> (in range [1..2]) of PID\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4]);\n");
  sendback(filedes,"CTRLLOOP:CH:PID:INVERTMEAS? <nch> <instance>\n");
  sendback(filedes,"                              : queries whether the measurement input is inverted\n");
  sendback(filedes,"                                in instance <instance> (in range [1..2]) of PID\n");
  sendback(filedes,"                                in ctrl loop channel <nch> (in range [1..4]);\n");
  sendback(filedes,"                                answer is {ON|OFF}\n");
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
  else if(strcmp(p,"DAC:CH")==0)
    parse_DACCH(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if( (strcmp(p,"DAC:CH:OFFS")==0) || (strcmp(p,"DAC:CH:OFFSET")==0) )
    parse_DACOFFS(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if( (strcmp(p,"DAC:CH:OUT_SEL")==0) || (strcmp(p,"DAC:CH:OUT_SELECT")==0) )
    parse_DAC_OUT_SELECT(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"ADC")==0)
    parse_READ_ADC(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if( (strcmp(p,"ADC:CH:OFFS")==0) || (strcmp(p,"ADC:CH:OFFSET")==0) )
    parse_ADCOFFS(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if( (strcmp(p,"ADC:CH:G")==0) || (strcmp(p,"ADC:CH:GAIN")==0) )
    parse_ADCGAIN(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"FSAMPL")==0)
    parse_FSAMPL(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"WAVEGEN")==0)
    parse_WAVEGEN(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"WAVEGEN:CH:CONFIG")==0)
    parse_WAVEGEN_CH_CONFIG(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"WAVEGEN:CH:STATE")==0)
    parse_WAVEGEN_CH_STATE(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"CTRLLOOP")==0)
    parse_CTRLLOOP(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"CTRLLOOP:CH:PID:RESET")==0)
    parse_PIDRESET(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"CTRLLOOP:CH:IIR:RESET")==0)
    parse_IIRRESET(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"CTRLLOOP:CH:IIR:COEFF")==0)
    parse_IIRCOEFF(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"CTRLLOOP:MATRIXROW")==0)
    parse_MATRIXROW(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"CTRLLOOP:CH:STATE")==0)
    parse_CTRLLOOP_CH_STATE(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if( (strcmp(p,"CTRLLOOP:CH:IN_SEL")==0) || (strcmp(p,"CTRLLOOP:CH:IN_SELECT")==0) )
    parse_CTRLLOOP_CH_INSEL(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if( (strcmp(p,"CTRLLOOP:CH:PID:G")==0) || (strcmp(p,"CTRLLOOP:CH:PID:GAINS")==0) )
    parse_PIDGAINS(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if( (strcmp(p,"CTRLLOOP:CH:PID:THR")==0) || (strcmp(p,"CTRLLOOP:CH:PID:THRESHOLDS")==0) )
    parse_PIDTHRESH(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"CTRLLOOP:CH:PID:PVDERIV")==0)
    parse_PID_PVDERIV(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"CTRLLOOP:CH:PID:INVERTCMD")==0)
    parse_PID_INVCMD(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"CTRLLOOP:CH:PID:INVERTMEAS")==0)
    parse_PID_INVMEAS(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
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

void split_lines(const char *buf, size_t len, int filedes, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  // split incoming message line by line and feed the lines to the parser
  // beware that a line may be split between successive reads

  static char leftover[SCPI_MAXMSG] = {0};
  static size_t leftover_len = 0;
  char answer[SCPI_MAXMSG];

  char temp[SCPI_MAXMSG * 2]; // to hold leftover + current buffer
  size_t temp_len = 0;

  // Combine leftover with new buffer
  memcpy(temp, leftover, leftover_len);
  temp_len = leftover_len;
  memcpy(temp + temp_len, buf, len);
  temp_len += len;
  temp[temp_len] = '\0';

  // Reset leftover
  leftover_len = 0;
  leftover[0] = '\0';

  // Process lines
  char *start = temp;
  char *newline;
  while((newline = memchr(start, '\n', temp + temp_len - start)))
    {
    *newline = '\0';

    // Pass complete line to callback
    //fprintf(stderr, "Incoming msg: '%s'(%d)\n", start,start[0]);
    parse(start, answer, SCPI_MAXMSG, filedes, endp_ptr, rpmsg_ptr);
    sendback(filedes, answer);

    start = newline + 1;
    }

  // Store any incomplete line as leftover
  if(start < temp + temp_len)
    {
    leftover_len = temp + temp_len - start;
    if(leftover_len >= SCPI_MAXMSG)
      leftover_len = SCPI_MAXMSG - 1; // truncate if too long
    memcpy(leftover, start, leftover_len);
    leftover[leftover_len] = '\0';
    }
  }


//-------------------------------------------------------------------
int SCPI_read_from_client(int filedes, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char buffer[SCPI_MAXMSG];
  int  nbytes;
  
  nbytes = read(filedes, buffer, SCPI_MAXMSG-1);  // "-1" to leave space for a \0
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
    // split it line by line and feed the lines to the parser
    // beware that a line may be split between successive reads
    split_lines(buffer, nbytes, filedes, endp_ptr, rpmsg_ptr);
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
