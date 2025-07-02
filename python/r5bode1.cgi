#!/usr/bin/python

import socket
import os
import urllib.parse
import time

# max samples per channel in shared memory
MAXSAMPLES=16383

# defaults
sweep_ch=4
in_ch=4
out_ch=1
fstart=100
fstop=4000
acquirenow=False


# --------- open a connection to r5ctrlr SCPI server ----------
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#s.connect(("127.0.0.1", 8888))
s.connect(("192.168.0.18", 8888))


# ---------  get the query string of the GET form  ------------
# It is passed to cgi scripts as the environment
# variable QUERY_STRING
query_string = os.environ['QUERY_STRING']
#query_string = ''
# convert the query string to a dictionary
arguments = urllib.parse.parse_qs(query_string)

# ---------  check the fields of the GET form query string ----
# ---------            and act accordingly                 ----
for name in arguments.keys():
  # -------- new sampling frequency
  if name=='fsampl':
    v_fsampl=arguments[name][0]
    cmd_s='FSAMPL '+str(v_fsampl)+'\n'
    # print(cmd_s)
    s.sendall(cmd_s.encode('ascii')) 
    ans=(s.recv(1024)).decode('utf-8')
    tok=ans.split(" ",2)
    if tok[0].strip()=='ERR:':
      print('Error commanding the new sampling frequency')
      # print(f'new commanded sampling freq is {v_fsampl} Hz')

  # -------- new DAC sweep channel
  elif name=='sweep_ch':
    sweep_ch=int(arguments[name][0])

  # -------- new ADC sys in channel
  elif name=='in_ch':
    in_ch=int(arguments[name][0])

  # -------- new ADC sys out channel
  elif name=='out_ch':
    out_ch=int(arguments[name][0])

  # -------- new fstart
  elif name=='fstart':
    fstart=int(arguments[name][0])

  # -------- new fstop
  elif name=='fstop':
    fstop=int(arguments[name][0])

  #------------ start a new acquisition -------------
  elif name=='startacq':
    if arguments[name][0].strip()=='1':
      acquirenow=True


  # ---------- just ignore unknown parameters
#    else:
#      print('unknown parameter')



# ---------  get current config  -------------

# get sampling freq
s.sendall(b"FSAMPL?\n") 
ans=(s.recv(1024)).decode("utf-8")
tok=ans.split(" ",2)
if(tok[0].strip()=="OK:"):
  fsampl=float(tok[1])
#else:
#  fsampl='&ltERR&gt'

acqtime=MAXSAMPLES/fsampl

# ----------  start html  ---------------------

print('Content-type:text/html\r\n\r\n')
print('<!DOCTYPE html>')
print('<html>')
print('<head>')
print('  <table>')
print('    <tr>')
print('      <td> <a href="r5bode1.cgi"> <img src="/MAX-IV_logo1_rgb-300x104.png" alt="MaxIV Laboratory"> </a> </td>')
print('      <td>')
print('      <H1>Max IV R5 controller</H1>')
print('      <H2>Bode Facility</H2>')
print('      </td>')
print('    </tr>')
print('  </table>')
print('</head>')

print('<body>')

print('<form action="" method="GET" >')

print('  <table>')

#-------  Fsampl  -----------------------------
print('    <tr>')
print('      <td>Sampling Frequency:</td>')
print('      <td>')
print(f'          <input type="number" name="fsampl" id="fsampl" value={tok[1]} min="1" max="10000" step=1 onchange="javascript:this.form.submit()"> Hz')
print('      </td>')
print('    </tr>')

#-------  Fstart  -----------------------------
print('    <tr>')
print('      <td>Bode start freq:</td>')
print('      <td>')
print(f'          <input type="number" name="fstart" id="fstart" value={fstart} min="1" max="10000" step=1 > Hz')
print('      </td>')
print('    </tr>')

#-------  Fstop  -----------------------------
print('    <tr>')
print('      <td>Bode end freq:</td>')
print('      <td>')
print(f'          <input type="number" name="fstop" id="fstop" value={fstop} min="1" max="10000" step=1 > Hz')
print('      </td>')
print('    </tr>')

#-------  Sweep Chan  -----------------------------
print('    <tr>')
print('      <td>Sweep (DAC) channel:</td>')
print('      <td>')
print(f'          <input type="number" name="sweep_ch" id="sweep_ch" value={sweep_ch} min="1" max="4" step=1 >')
print('      </td>')
print('    </tr>')

#-------  IN Chan  -----------------------------
print('    <tr>')
print('      <td>System IN (ADC) channel:</td>')
print('      <td>')
print(f'          <input type="number" name="in_ch" id="in_ch" value={in_ch} min="1" max="4" step=1 >')
print('      </td>')
print('    </tr>')

#-------  OUT Chan  -----------------------------
print('    <tr>')
print('      <td>System OUT (ADC) channel:</td>')
print('      <td>')
print(f'          <input type="number" name="out_ch" id="out_ch" value={out_ch} min="1" max="4" step=1 >')
print('      </td>')
print('    </tr>')


print('  </table>')

print(f'  <br>Acquisition time in this configuration is {acqtime} sec<br><br>')

print('  <button type="submit" name="startacq" id="startacq" value=1> Acquire </button>')

print('</form>')

if acquirenow:
  print('  <br> >>> Acquiring <<< <br><br>')
  
print('</body>')
print('</html>')

s.close


