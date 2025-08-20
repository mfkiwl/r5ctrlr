#!/usr/bin/python

import socket
#import select
import os
import urllib.parse
#import time
#from datetime import datetime
import numpy as np
#import matplotlib.pyplot as plt
#import io
#import base64
#import fnmatch

# constants


# defaults


# ----------  html header  ---------------------

print('Content-type:text/html\r\n\r\n')
print('<!DOCTYPE html>')
print('<html>')
print('<head>')
print('')
print('  <table>')
print('    <tr>')
print('      <td> <a href="/cgi-bin/index.cgi"> <img src="/MAX-IV_logo1_rgb-300x104.png" alt="MaxIV Laboratory"> </a> </td>')
print('      <td>')
print('      <H1>Max IV R5 controller</H1>')
print('      <H2>Notch IIR coefficients calculation utility</H2>')
print('      </td>')
print('    </tr>')
print('  </table>')
print('</head>')

print('<body>')


# --------- open a connection to r5ctrlr SCPI server ----------
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 8888))
#s.connect(("192.168.0.18", 8888))


# ----- get sampling freq
s.sendall(b"FSAMPL?\n") 
ans=(s.recv(1024)).decode("utf-8")
tok=ans.split(" ",2)
if(tok[0].strip()=="OK:"):
  fsampl=float(tok[1])
else:
  fsampl=10000.0




# ------------------  do the changes requested by the GET form query string  --------------------

# the query string of the GET form
# It is passed to cgi scripts as the environment
# variable QUERY_STRING
query_string = os.environ['QUERY_STRING']
#query_string = ''
# convert the query string to a dictionary
arguments = urllib.parse.parse_qs(query_string)

# ---------  check the fields of the GET form query string ----
# ---------            and act accordingly                 ----

# take note of what channel we change, because we'll have to make some calculations
# and R5 update after reading all the settings from the GET query string

#sensible defaults
f0=fsampl/10
zeta=0.71
ch=1
iirinst=1
calccoeff=False
queryok=True

for name in arguments.keys():

  if name=='f0':
    f0=float(arguments[name][0])
  elif name=='zeta':
    zeta=float(arguments[name][0])
  elif name=='ch':
    i=int(arguments[name][0])
    if((i>=1)and(i<=4)):
      ch=i
    else:
      print('<br>invalid channel<br>')
      queryok=False
  elif name=='iirinst':
    i=int(arguments[name][0])
    if((i>=1)and(i<=2)):
      iirinst=i
    else:
      print('<br>invalid instance<br>')
      queryok=False
  elif name=='calccoeff':
    i=int(arguments[name][0])
    calccoeff= (i==1)


# ---------       now workout the low-level coefficients          ------------
# ---------  and write back the updated channel config to the R5  ------------

if( calccoeff and queryok ):
  w0=2*np.pi*f0
  # prewarp
  w0=2.*fsampl*np.tan(w0/(2.*fsampl))
  # coefficient worked out using a bilinear transformation = Tustin
  alpha0= 1.+4.*fsampl**2/w0**2
  alpha1= 2.-8.*fsampl**2/w0**2
  alpha2= alpha0
  beta0= 1+ 4.*zeta*fsampl/w0 + 4.*fsampl**2/w0**2
  beta1= alpha1
  beta2= 1- 4.*zeta*fsampl/w0 + 4.*fsampl**2/w0**2
  A0= alpha0/beta0
  A1= alpha1/beta0
  A2= alpha2/beta0
  B1= beta1/beta0
  B2= beta2/beta0

  qstr='CTRLLOOP:CH:IIR:COEFF '+str(ch)+' '+str(iirinst)+' '        \
                               +str(A0)+' '+str(A1)+' '+str(A2)+' ' \
                               +str(B1)+' '+str(B2)+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  tok=ans.split(" ",2)
  if(tok[0].strip()=="OK"):
    print(f'<br>Successfully updated IIR#{iirinst} in ch#{ch} with these coeff:<br><br>')
    print('<table>')
    print('  <tr>')
    print('    <td>')
    print('      Y(n)= ')
    print('    </td>')
    print('    <td>')
    print(f'     {A0:+.8}')
    print('    </td>')
    print('    <td>')
    print('      &bull;')
    print('    </td>')
    print('    <td>')
    print('      X(n)')
    print('    </td>')
    print('  </tr>')
    
    print('  <tr>')
    print('    <td>')
    print('      ')
    print('    </td>')
    print('    <td>')
    print(f'     {A1:+.8}')
    print('    </td>')
    print('    <td>')
    print('      &bull;')
    print('    </td>')
    print('    <td>')
    print('      X(n-1)')
    print('    </td>')
    print('  </tr>')
    
    print('  <tr>')
    print('    <td>')
    print('      ')
    print('    </td>')
    print('    <td>')
    print(f'     {A2:+.8}')
    print('    </td>')
    print('    <td>')
    print('      &bull;')
    print('    </td>')
    print('    <td>')
    print('      X(n-2)')
    print('    </td>')
    print('  </tr>')

    print('  <tr>')
    print('    <td>')
    print('      ')
    print('    </td>')
    print('    <td>')
    print(f'     {-B1:+.8}')
    print('    </td>')
    print('    <td>')
    print('      &bull;')
    print('    </td>')
    print('    <td>')
    print('      Y(n-1)')
    print('    </td>')
    print('  </tr>')

    print('  <tr>')
    print('    <td>')
    print('      ')
    print('    </td>')
    print('    <td>')
    print(f'     {-B2:+.8}')
    print('    </td>')
    print('    <td>')
    print('      &bull;')
    print('    </td>')
    print('    <td>')
    print('      Y(n-2)')
    print('    </td>')
    print('  </tr>')

    print('</table>')
  else:
    print(f'<br>error updating IIR#{iirinst} in ch#{ch}<br>')

print('<br><br>')



# --------------------  now display html page  -----------------------

print('General form of notch transfer function:<br><br>')
print('<img width="180" height="40" src="/notch_formula.png" alt="Notch formula in frequency domain">')
print('<br><br>')
print('I remember how the damping factor &zeta; is related to the quality factor Q and the -3dB bandwidth ')
print('(i.e. the bandwidth where the amplitude is -3dB, not 3dB from max attenuation):')
print('<br><br>')
print('<img width="120" height="40" src="/notch_zeta.png" alt="dF/F0=2zeta=1/Q">')
print('<br>')
print('<img width="560" height="420" src="/notch.png" alt="Typical notch Bode diagram for different &zeta;">')
print('<br>')
print(f'Controller sampling frequency is {fsampl} Hz.')
print('<br>')
print('Please use the controller setup page if you want to change it before calculating the IIR coefficients.')
print('<br>')
print('<br>')

print('<form action="" method="GET" >')

# -- Prevent implicit submission of the form hitting <enter> key
print('  <button type="submit" disabled style="display: none" aria-hidden="true"></button>')


print('  <table>')

#-------  f0  -----------------------------
print('    <tr>')
print('      <td>Notch frequency f<sub>0</sub> (=&omega;<sub>0</sub>/2&pi;) =</td>')
print('      <td>')
print(f'          <input type="number" name="f0" id="f0" value="{f0}" min="1" max="{fsampl/2.}" step="any"> Hz')
print('      </td>')
print('    </tr>')

#-------  zeta  -----------------------------
print('    <tr>')
print('      <td>&zeta; =</td>')
print('      <td>')
print(f'          <input type="number" name="zeta" id="zeta" value="{zeta}" step="any">')
print('      </td>')
print('    </tr>')

#-------  Channel number  -----------------------------
print('    <tr>')
print('      <td>Channel number:</td>')
print('      <td>')
print('        <select name="ch" id = "ch">')
for i in range(1,5):
  print(f'          <option value = "{i}" ')
  if( i==ch ):
    print('selected="selected"')
  print(f'>{i}</option>')
print('        </select>')
print('      </td>')
print('    </tr>')

#-------  IIR instance  -----------------------------
print('    <tr>')
print('      <td>IIR instance:</td>')
print('      <td>')
print('        <select name="iirinst" id = "iirinst">')
for i in range(1,3):
  print(f'          <option value = "{i}" ')
  if( i==iirinst ):
    print('selected="selected"')
  print(f'>{i}</option>')
print('        </select>')
print('      </td>')
print('    </tr>')


#-------  Submit button  -----------------------------
print('    <tr><td colspan="2">')
print(f'     <button type="submit" name="calccoeff" id="calccoeff" value=1> Calculate IIR coeff and update controller</button>')
print('    </td></tr>')


print('  </table>')
print('</form>')

print('<br><br>')
print('</body>')
print('</html>')

s.close


