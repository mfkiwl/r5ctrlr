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

#ifndef SCPISERVER_H
#define SCPISERVER_H

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <openamp/open_amp.h>
#include "common.h"
#include "linux_main.h"


#define SCPI_PORT    8888
#define SCPI_MAXMSG  512

#define SCPI_READ  1
#define SCPI_WRITE 0
#define SCPI_ERRS "ERR"
#define SCPI_OKS  "OK"

#define SCPI_NO_ERR    0
#define SCPI_GEN_ERR  -1

#define PRODUCT_FNAME "/etc/petalinux/product"
#define VERSION_FNAME "/etc/petalinux/version"

#define FILTERTYPE_PID   1
#define FILTERTYPE_IIR   2


// ##########  types  #######################

// ##########  extern globals  ################

// ##########  protos  ########################
void upstring(char *s);
void trimstring(char* s);
void                      parse_IDN(char *ans, size_t maxlen);
void                      parse_STB(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                      parse_RST(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                      parse_DAC(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                    parse_DACCH(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                  parse_DACOFFS(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void           parse_DAC_OUT_SELECT(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                 parse_READ_ADC(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                  parse_ADCOFFS(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                  parse_ADCGAIN(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                   parse_FSAMPL(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                  parse_WAVEGEN(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void        parse_WAVEGEN_CH_CONFIG(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void         parse_WAVEGEN_CH_STATE(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                 parse_CTRLLOOP(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void do_FilterReset(int filtertype, char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                 parse_PIDRESET(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                 parse_IIRRESET(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void        parse_CTRLLOOP_CH_STATE(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void        parse_CTRLLOOP_CH_INSEL(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                 parse_PIDGAINS(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                     parse_TRIG(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void                parse_TRIGSETUP(char *ans, size_t maxlen, int rw, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void printSamples(int filedes);
void printHelp(int filedes);
void parse(char *buf, char *ans, size_t maxlen, int filedes, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
void sendback(int filedes, char *s);
int  SCPI_read_from_client(int filedes, RPMSG_ENDP_TYPE *endp_ptr, R5_RPMSG_TYPE *rpmsg_ptr);
int  startSCPIserver(int *sock_ptr, fd_set *active_fd_set_ptr);

#endif
