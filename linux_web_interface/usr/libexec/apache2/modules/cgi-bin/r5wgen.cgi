#!/usr/bin/python

import socket
import select
import os
import urllib.parse
import time
from datetime import datetime
import numpy as np
import matplotlib.pyplot as plt
import io
import base64

# constants


# defaults


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
      print('<br>Error commanding the new sampling frequency<br>')
      # print(f'new commanded sampling freq is {v_fsampl} Hz')

  # -------- new DAC sweep channel
  elif name=='sweep_ch':
    sweep_ch=int(arguments[name][0])


  # ---------- just ignore unknown parameters
#    else:
#      print('unknown parameter')



# ---------  get current config  -------------

# ----- get sampling freq
s.sendall(b"FSAMPL?\n") 
ans=(s.recv(1024)).decode("utf-8")
tok=ans.split(" ",2)
if(tok[0].strip()=="OK:"):
  fsampl=float(tok[1])
else:
  fsampl=10000

# ----- get global ON/OFF state
s.sendall(b"STB?\n") 
ans=(s.recv(1024)).decode("utf-8")
tok=ans.split(" ",2)
if(tok[0].strip()=="OK:"):
  if (tok[1].strip()=="WAVEGEN"):
    globalEnable=True
  else:
    globalEnable=False
else:
  globalEnable=False


# ----- get channel config

ch_conf=[dict() for ch in range(4)]

for ch in range(4):
  qstr='WAVEGEN:CH_ENABLE? '+str(ch+1)+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  tok=ans.split(" ",2)
  if(tok[0].strip()=="OK:"):
    ch_conf[ch]['en']=tok[1].strip()
  else:
    ch_conf[ch]['en']='OFF'

  qstr='WAVEGEN:CH_CONFIG? '+str(ch+1)
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  tok=ans.split(" ",7)
  if(tok[0].strip()=="OK:"):
    ch_conf[ch]['type']=tok[1].strip()     # DC/SINE/SWEEP
    ch_conf[ch]['ampl']=float(tok[2].strip())
    ch_conf[ch]['offs']=float(tok[3].strip())
    ch_conf[ch]['f1']  =float(tok[4].strip())
    ch_conf[ch]['f2']  =float(tok[5].strip())
    ch_conf[ch]['dt']  =float(tok[6].strip())
  else:
    ch_conf[ch]['type']="DC"
    ch_conf[ch]['ampl']=0.0
    ch_conf[ch]['offs']=0.0
    ch_conf[ch]['f1']  =0.0
    ch_conf[ch]['f2']  =0.0
    ch_conf[ch]['dt']  =1.0              # sensible default to avoid divisions by 0



# ----------  start html  ---------------------

print('Content-type:text/html\r\n\r\n')
print('<!DOCTYPE html>')
print('<html>')
print('<head>')
print('  <table>')
print('    <tr>')
print('      <td> <a href="?"> <img src="/MAX-IV_logo1_rgb-300x104.png" alt="MaxIV Laboratory"> </a> </td>')
print('      <td>')
print('      <H1>Max IV R5 controller</H1>')
print('      <H2>Waveform Generator Facility</H2>')
print('      </td>')
print('    </tr>')
print('  </table>')
print('</head>')

print('<body>')

print('<form action="" method="GET" >')

# -- Prevent implicit submission of the form hitting <enter> key
print('  <button type="submit" disabled style="display: none" aria-hidden="true"></button>')

print('  <table>')

#-------  Fsampl  -----------------------------
print('    <tr>')
print('      <td>Sampling Frequency:</td>')
print('      <td>')
print(f'          <input type="number" name="fsampl" id="fsampl" value={fsampl} min="1" max="10000" step=1 onchange="javascript:this.form.submit()"> Hz')
print('      </td>')
print('    </tr>')

#-------  Global ON/OFF  -----------------------------
print('    <tr>')
print('      <td>Global Enable:</td>')
print('      <td>')
print('        <select name="globenbl" id = "globenbl" onchange="javascript:this.form.submit()">')
if(globalEnable==False):
  print('          <option value = "0" selected="selected">OFF</option>')
  print('          <option value = "1">ON</option>')
else:
  print('          <option value = "0">OFF</option>')
  print('          <option value = "1" selected="selected">ON</option>')
print('        </select>')
print('      </td>')
print('    </tr>')

#-------  spacer  -----------------------------
print('    <tr><td colspan="3" class="divider"><hr /></td></tr>')

#-------  channel conf  -----------------------------

for ch in range(4):
  print('    <tr>')
  print(f'      <td>Channel {ch+1} Configuration:</td>')
  print('    </tr>')

#-------  channel type  -----------------------------
  print('    <tr>')
  print('      <td>')
  print('      </td>')
  print('      <td>Type:</td>')
  print('      <td>')
  print(f'        <select name="type{ch}" id = "type{ch}">')
  if ch_conf[ch]['type']=="DC" :
    print('          <option value = "0" selected>DC</option>')
    print('          <option value = "1">SINE</option>')
    print('          <option value = "2">SWEEP</option>')
  elif ch_conf[ch]['type']=="SINE":
    print('          <option value = "0">DC</option>')
    print('          <option value = "1" selected>SINE</option>')
    print('          <option value = "2">SWEEP</option>')
  else:
    print('          <option value = "0">DC</option>')
    print('          <option value = "1">SINE</option>')
    print('          <option value = "2" selected>SWEEP</option>')
  print('        </select>')
  print('      </td>')
  print('    </tr>')

#-------  channel amplitude  -----------------------------
  print('    <tr>')
  print('      <td>')
  print('      </td>')
  print('      <td>Amplitude (range [-1,1]):</td>')
  print('      <td>')
  print(f'       <input type="number" name="amp{ch}" id="amp{ch}" value={ch_conf[ch]["ampl"]:.3f} size="15em" min="-1.0" max="+1.0">')
  print('      </td>')
  print('    </tr>')

#-------  channel offset  -----------------------------
  print('    <tr>')
  print('      <td>')
  print('      </td>')
  print('      <td>Offset (range [-1,1]):</td>')
  print('      <td>')
  print(f'       <input type="number" name="offs{ch}" id="offs{ch}" value={ch_conf[ch]["offs"]:.3f} size="15em" min="-1.0" max="+1.0">')
  print('      </td>')
  print('    </tr>')

#-------  channel f1  -----------------------------
  print('    <tr>')
  print('      <td>')
  print('      </td>')
  print('      <td>f1 (Hz):</td>')
  print('      <td>')
  print(f'       <input type="number" name="fone{ch}" id="fone{ch}" value={ch_conf[ch]["f1"]:.3f} size="15em" min="0.0" max="10000.0" >')
  print('      </td>')
  print('    </tr>')

#-------  channel f2  -----------------------------
  print('    <tr>')
  print('      <td>')
  print('      </td>')
  print('      <td>f2 (Hz):</td>')
  print('      <td>')
  print(f'       <input type="number" name="ftwo{ch}" id="ftwo{ch}" value={ch_conf[ch]["f2"]:.3f} size="15em" min="0.0" max="10000.0" >')
  print('      </td>')
  print('    </tr>')

#-------  channel sweeptime  -----------------------------
  print('    <tr>')
  print('      <td>')
  print('      </td>')
  print('      <td>sweep time (s):</td>')
  print('      <td>')
  print(f'       <input type="number" name="dt{ch}" id="dt{ch}" value={ch_conf[ch]["dt"]:.3f} size="15em" min="0.0" >')
  print('      </td>')
  print('    </tr>')

#-------  spacer  -----------------------------
  print('    <tr><td colspan="3" class="divider"><hr /></td></tr>')

print('  </table>')

print(f'  <button type="submit" name="updconf" id="updconf" value=1> update configuration </button>')

print('</form>')

print('<br><br>')
print('</body>')
print('</html>')

s.close


