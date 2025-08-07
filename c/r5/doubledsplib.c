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

#include "doubledsplib.h"

// ##########  globals  #######################

// ##########  implementation  ################

// generic 2nd order IIR
// Y(n) = a0*X(n) + a1*X(n-1) + a2*X(n-2) +
//                - b1*Y(n-1) - b2*Y(n-2)
//
// (note the minus sign on Y coefficients)

double IIR2(double x, IIR2_COEFF *coeff)
  {
  double y;

  y = coeff->a[0]*x + coeff->a[1]*coeff->x[0] + coeff->a[2]*coeff->x[1] 
                    - coeff->b[0]*coeff->y[0] - coeff->b[1]*coeff->y[1];
  
  coeff->x[1] = coeff->x[0];
  coeff->x[0] = x;
  coeff->y[1] = coeff->y[0];
  coeff->y[0] = y;
  
  return y;
  }


// -----------------------------------------------------------

// multiple in, single out
// x is a double vector of dimension dim
// a and b have dimension (dim+1)
// out = (a0+a1*x1+...+an*xn) / (b0+b1*x1+...+bn*xn)

double MISO(double *x, float *a, float *b, int dim)
  {
  int i;
  double num,den;
  num=(double)(*a);
  den=(double)(*b);
  for(i=0;i<dim;i++)
    {
    num=num+(*(a+i+1))*(*(x+i));
    den=den+(*(b+i+1))*(*(x+i));
    }
  return (num/den);
  }


// -----------------------------------------------------------

double PID(double command, double meas, PID_GAINS *coeff)
  {
  double x, xd, yp, yi, yd, y, ysat, G2Dcorr, cmd, act;

  // invert inputs if needed
  if(coeff->invert_cmd)
    cmd=-command;
  else
    cmd=command;

  if(coeff->invert_meas)
    act=-meas;
  else
    act=meas;

  // error node
  x=cmd-act;

  // input dead band
  if((fabs(x) <= coeff->in_thr))
    {
    x=0.;
    // when in dead band, leave the filter decay only in the derivative term (i.e. put G2d=0)
    // if the derivative is on the error node
    if(coeff->deriv_on_PV)
      G2Dcorr=(double)(coeff->G2d);
    else
      G2Dcorr=0.;
    }
  else
    {
    G2Dcorr=(double)(coeff->G2d);
    }

  // prop
  yp = x*coeff->Gp;
  
  // integr
  yi = coeff->yi_n1 + coeff->Gi*(x + coeff->xn1);

  // deriv
  // remember to use -meas and not meas when doing the derivative on the process variable
  if(coeff->deriv_on_PV)
    yd = coeff->G1d * coeff->yd_n1 + G2Dcorr*(-act - coeff->measn1);
  else
    yd = coeff->G1d * coeff->yd_n1 + G2Dcorr*(   x  - coeff->xn1);


  y = yp + yi + yd;

  // output saturation
  ysat=y;
  if(ysat >= coeff->out_sat)
    ysat=coeff->out_sat;
  else if(ysat <= -coeff->out_sat)
    ysat=-coeff->out_sat;

  // update pipeline
  coeff->xn1   = x;
  coeff->measn1=-act;
  // anti-windup correction for integral part
  coeff->yi_n1 = yi + (ysat-y)*coeff->G_aiw;
  coeff->yd_n1 = yd;

  return ysat;
  }



