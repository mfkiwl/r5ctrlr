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
#include "common.h"


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


// ##########  types  #######################

// ##########  extern globals  ################

// ##########  protos  ########################
void         upstring(char *s);
void         trimstring(char* s);
void         parseIDN(char *ans, size_t maxlen);
void         parseSTB(char *ans, size_t maxlen);
void         parseRST(char *ans, size_t maxlen);
void         printHelp(int filedes);
void         parse(char *buf, char *ans, size_t maxlen, int filedes);
void         sendback(int filedes, char *s);
int          SCPI_read_from_client(int filedes);
int          startSCPIserver(int *sock_ptr, fd_set *active_fd_set_ptr);
#endif
