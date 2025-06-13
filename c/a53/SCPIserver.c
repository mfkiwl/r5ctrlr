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
  snprintf(ans, maxlen, "%s - version %s\n", prod, ver);
  }


//-------------------------------------------------------------------

void parse_STB(char *ans, size_t maxlen)
  {
   snprintf(ans, maxlen, 
            "%s: ciccibum\n", SCPI_OKS
           );
  }


//-------------------------------------------------------------------

void parse_RST(char *ans, size_t maxlen)
  {
  snprintf(ans, maxlen, "%s: pippo pluto paperino\n", SCPI_OKS);        
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

void parse_READ_ADC(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr)
  {
  char *p;
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

void printHelp(int filedes)
  {
  sendback(filedes,"R5 controller SCPI server commands\n\n");
  sendback(filedes,"Server support multiple concurrent clients\n");
  sendback(filedes,"Server is case insensitive\n");
  sendback(filedes,"Numbers can be decimal or hex, with the 0x prefix\n");
  sendback(filedes,"Server answers with OK or ERR, a colon and a descriptive message\n");
  sendback(filedes,"Send CTRL-D to close the connection\n\n");
  sendback(filedes,"Command list:\n\n");
  sendback(filedes,"*IDN?                         : print firmware name and version\n");
  sendback(filedes,"*STB?                         : ciccibum\n");
  sendback(filedes,"*RST                          : pippo pluto paperino\n");
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
    parse_STB(ans, maxlen);
  else if(strcmp(p,"*RST")==0)
    parse_RST(ans, maxlen);
  else if(strcmp(p,"DAC")==0)
    parse_DAC(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
  else if(strcmp(p,"ADC")==0)
    parse_READ_ADC(ans, maxlen, rw, endp_ptr, rpmsg_ptr);
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
