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

// ##########  types  #######################
typedef struct
  {
  // user coeff
  double a[3]; // x_i coeff
  double b[2]; // y_i coeff
  // internal "static" vars
  double x[2]; // x pipeline
  double y[2]; // y pipeline
  } IIR2_COEFF;

typedef struct
  {
  // user gains
  double Gp;         // prop gain
  double Gi;         // integr gain
  double G1d;        // deriv gain #1
  double G2d;        // deriv gain #2
  double G_aiw;      // anti integral windup gain
  double out_sat;    // output saturation limit
  double in_thr;     // input dead band
  bool deriv_on_PV;  // derivative on process variable?
  bool invert_cmd;   // invert commanded value
  bool invert_meas;  // invert measured value
  // internal "static" vars
  double xn1;        // x(n-1)
  double yi_n1;      // integral part at previous step = y_I(n-1)
  double yd_n1;      // derivative part at previous step = y_D(n-1)
  } PID_GAINS;

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
