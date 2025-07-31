//
// R5 integrated controller 
// ZCU102 + ADI CN0585/CN0584
//
// IRQs from PL and 
// interproc communications with A53/linux
//
//////////////////////////////////////////
//
// implementation of a DSP lib
// in double precision floating point
//

#ifndef DOUBLEDSPLIB_H_
#define DOUBLEDSPLIB_H_

#include <math.h>
#include <stdbool.h>
// I need r5_main.h to have ARMR5 #defined
#include "r5_main.h"
#include "common.h"

// ##########  types  #######################

// ##########  extern globals  ################

// ##########  protos  ########################

// generic 2nd order IIR
// Y(n) = a0*X(n) + a1*X(n-1) + a2*X(n-2) +
//                - b1*Y(n-1) - b2*Y(n-2)
//
// (note the minus sign on Y coefficients)
double IIR2(double x, IIR2_COEFF *coeff);

// multiple in, single out
// x is a double vector of dimension dim
// a and b have dimension (dim+1)
// out = (a0+a1*x1+...+an*xn) / (b0+b1*x1+...+bn*xn)
double MISO(double *x, double *a, double *b, int dim);

double PID(double command, double meas, PID_GAINS *coeff);

#endif
