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
import fnmatch

# constants


# defaults


# ----------  html header  ---------------------

print('Content-type:text/html\r\n\r\n')
print('<!DOCTYPE html>')
print('<html>')
print('<head>')
print('  <table>')
print('    <tr>')
print('      <td> <a href="/cgi-bin/index.cgi"> <img src="/MAX-IV_logo1_rgb-300x104.png" alt="MaxIV Laboratory"> </a> </td>')
print('      <td>')
print('      <H1>Max IV R5 controller</H1>')
print('      <H2>Waveform Generator Facility</H2>')
print('      </td>')
print('    </tr>')
print('  </table>')
print('</head>')

print('<body>')


# --------- open a connection to r5ctrlr SCPI server ----------
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 8888))
#s.connect(("192.168.0.18", 8888))



# ---------  first of all, get current config  -------------

# ----- get sampling freq
s.sendall(b"FSAMPL?\n") 
ans=(s.recv(1024)).decode("utf-8")
tok=ans.split(" ",2)
if(tok[0].strip()=="OK:"):
  fsampl=float(tok[1])
else:
  fsampl=10000.0

# ----- get global ON/OFF state
s.sendall(b"*STB?\n") 
ans=(s.recv(1024)).decode("utf-8")
tok=ans.split(" ",2)
if(tok[0].strip()=="OK:"):
  stb=int(tok[1])
  if((stb&0x02) != 0):
    globalEnable=True
  else:
    globalEnable=False
else:
  globalEnable=False


# ----- get channel config

ch_conf=[dict() for ch in range(4)]

for ch in range(4):
  qstr='WAVEGEN:CH:STATE? '+str(ch+1)+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  tok=ans.split(" ",2)
  if(tok[0].strip()=="OK:"):
    ch_conf[ch]['en']=tok[1].strip()
  else:
    ch_conf[ch]['en']='OFF'

  qstr='WAVEGEN:CH:CONFIG? '+str(ch+1)+'\n'
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



# ---------  now get the config change requested by the GET form query string  ------------

# the query string of the GET form
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
    fsampl=float(arguments[name][0])
    cmd_s='FSAMPL '+str(fsampl)+'\n'
    # print(cmd_s)
    s.sendall(cmd_s.encode('ascii')) 
    ans=(s.recv(1024)).decode('utf-8')
    tok=ans.split(" ",2)
    if tok[0].strip()=='ERR:':
      print('<br>Error commanding the new sampling frequency<br>')
    #else:
    #  print(f'new commanded sampling freq is {v_fsampl} Hz')

  # -------- global enable state
  if name=='globenbl':
    if int(arguments[name][0])==1:
      cmd_s='WAVEGEN ON\n'
      globalEnable=True
    else:
      cmd_s='WAVEGEN OFF\n'
      globalEnable=False
    s.sendall(cmd_s.encode('ascii')) 
    ans=(s.recv(1024)).decode('utf-8')
    tok=ans.split(" ",2)
    if tok[0].strip()=='ERR:':
      print('<br>Error commanding the global ON/OFF state<br>')

  # -------- now check channel enable request; channel number is a suffix;
  # -------- I use fnmatch to test with wildcard
  
  # --------  channel enable  ----------
  elif fnmatch.fnmatch(name,'ch_enbl?'):
    the_chan=int(name[-1])
    if int(arguments[name][0])==0:
      the_state='OFF'
    elif int(arguments[name][0])==1:
      the_state='ON'
    elif int(arguments[name][0])==2:
      the_state='SINGLE'
    else:
      the_state='OFF'
    ch_conf[the_chan-1]['en']=the_state
    cmd_s='WAVEGEN:CH:STATE '+str(the_chan)+' '+the_state+'\n'
    #print(f'<br> >>>>> {cmd_s} <<<<< <br>')
    s.sendall(cmd_s.encode('ascii')) 
    ans=(s.recv(1024)).decode('utf-8')
    tok=ans.split(" ",2)
    if tok[0].strip()=='ERR:':
      print('<br>Error commanding channel ON/OFF/SINGLE state<br>')


  # -------- for channel config I update local variables and 
  # -------- update R5 config at the end, after the query string has 
  # -------- been fully parsed
  # -------- Also here I use fnmatch to test with wildcard

  # --------  channel type  ----------
  elif fnmatch.fnmatch(name,'type?'):
    the_chan=int(name[-1])
    if int(arguments[name][0])==0:
      ch_conf[the_chan-1]['type']="DC"
    elif int(arguments[name][0])==1:
      ch_conf[the_chan-1]['type']="SINE"
    elif int(arguments[name][0])==2:
      ch_conf[the_chan-1]['type']="SWEEP"
    else:
      ch_conf[the_chan-1]['type']="DC"
    
  # --------  channel ampl  ----------
  elif fnmatch.fnmatch(name,'ampl?'):
    the_chan=int(name[-1])
    ch_conf[the_chan-1]['ampl']=float(arguments[name][0])

  # --------  channel offs  ----------
  elif fnmatch.fnmatch(name,'offs?'):
    the_chan=int(name[-1])
    ch_conf[the_chan-1]['offs']=float(arguments[name][0])

  # --------  channel f1  ----------
  elif fnmatch.fnmatch(name,'fone?'):
    the_chan=int(name[-1])
    ch_conf[the_chan-1]['f1']=float(arguments[name][0])

  # --------  channel f2  ----------
  elif fnmatch.fnmatch(name,'ftwo?'):
    the_chan=int(name[-1])
    ch_conf[the_chan-1]['f2']=float(arguments[name][0])

  # --------  channel dt  ----------
  elif fnmatch.fnmatch(name,'dt?'):
    the_chan=int(name[-1])
    ch_conf[the_chan-1]['dt']=float(arguments[name][0])


  # ---------- just ignore unknown parameters
#    else:
#      print('unknown parameter')


# ---------  now write back the updated channel config to the R5  ------------

for ch in range(4):
  qstr='WAVEGEN:CH:CONFIG '+str(ch+1)+' '                \
                           +ch_conf[ch]['type']+' '      \
                           +str(ch_conf[ch]['ampl'])+' ' \
                           +str(ch_conf[ch]['offs'])+' ' \
                           +str(ch_conf[ch]['f1'])+' '   \
                           +str(ch_conf[ch]['f2'])+' '   \
                           +str(ch_conf[ch]['dt'])+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  #tok=ans.split(" ",2)
  #if(tok[0].strip()=="OK:"):
  #  print(f'updated ch {ch+1}\n')
  #else:
  #  print(f'error updating ch {ch+1}\n')



# --------------------  now display html page  -----------------------

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


#-------  Submit button  -----------------------------
print('    <tr>')
print('      <td>')
print(f'      <button type="submit" name="updconf" id="updconf" value=1> update configuration </button>')
print('      </td>')
print('    </tr>')


#-------  spacer  -----------------------------
print('    <tr><td colspan="3" class="divider"><hr /></td></tr>')

#-------  channel conf  -----------------------------

for ch in range(4):
  print('    <tr>')
  print(f'      <td>Channel {ch+1} Configuration:</td>')
  print('    </tr>')

#-------  channel enable  -----------------------------
  print('    <tr>')
  print('      <td>')
  print('      </td>')
  print('      <td>State:</td>')
  print('      <td>')
  print(f'        <select name="ch_enbl{ch+1}" id = "ch_enbl{ch+1}">')
  if ch_conf[ch]['en']=="OFF" :
    print('          <option value = "0" selected>OFF</option>')
    print('          <option value = "1"         >ON</option>')
    print('          <option value = "2"         >SINGLE</option>')
  elif ch_conf[ch]['en']=="ON" :
    print('          <option value = "0"         >OFF</option>')
    print('          <option value = "1" selected>ON</option>')
    print('          <option value = "2"         >SINGLE</option>')
  elif ch_conf[ch]['en']=="SINGLE" :
    print('          <option value = "0"         >OFF</option>')
    print('          <option value = "1"         >ON</option>')
    print('          <option value = "2" selected>SINGLE</option>')
  else:
    print('          <option value = "0" selected>OFF</option>')
    print('          <option value = "1"         >ON</option>')
    print('          <option value = "2"         >SINGLE</option>')
  print('        </select>')
  print('      </td>')
  print('    </tr>')

#-------  channel type  -----------------------------
  print('    <tr>')
  print('      <td>')
  print('      </td>')
  print('      <td>Type:</td>')
  print('      <td>')
  print(f'        <select name="type{ch+1}" id = "type{ch+1}">')
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
  print(f'       <input type="number" name="ampl{ch+1}" id="ampl{ch+1}" value={ch_conf[ch]["ampl"]:.3f} size="15em" min="-1.0" max="+1.0" step="any">')
  print('      </td>')
  print('    </tr>')

#-------  channel offset  -----------------------------
  print('    <tr>')
  print('      <td>')
  print('      </td>')
  print('      <td>Offset (range [-1,1]):</td>')
  print('      <td>')
  print(f'       <input type="number" name="offs{ch+1}" id="offs{ch+1}" value={ch_conf[ch]["offs"]:.3f} size="15em" min="-1.0" max="+1.0" step="any">')
  print('      </td>')
  print('    </tr>')

#-------  channel f1  -----------------------------
  print('    <tr>')
  print('      <td>')
  print('      </td>')
  print('      <td>f1 (Hz):</td>')
  print('      <td>')
  print(f'       <input type="number" name="fone{ch+1}" id="fone{ch+1}" value={ch_conf[ch]["f1"]:.3f} size="15em" min="0.0" max="10000.0" step="any">')
  print('      </td>')
  print('    </tr>')

#-------  channel f2  -----------------------------
  print('    <tr>')
  print('      <td>')
  print('      </td>')
  print('      <td>f2 (Hz):</td>')
  print('      <td>')
  print(f'       <input type="number" name="ftwo{ch+1}" id="ftwo{ch+1}" value={ch_conf[ch]["f2"]:.3f} size="15em" min="0.0" max="10000.0" step="any">')
  print('      </td>')
  print('    </tr>')

#-------  channel sweeptime  -----------------------------
  print('    <tr>')
  print('      <td>')
  print('      </td>')
  print('      <td>sweep time (s):</td>')
  print('      <td>')
  print(f'       <input type="number" name="dt{ch+1}" id="dt{ch+1}" value={ch_conf[ch]["dt"]:.3f} size="15em" min="0.0" step="any">')
  print('      </td>')
  print('    </tr>')

#-------  spacer  -----------------------------
  print('    <tr><td colspan="3" class="divider"><hr /></td></tr>')

print('  </table>')

#print(f'  <button type="submit" name="updconf" id="updconf" value=1> update configuration </button>')

print('</form>')

print('<br><br>')
print('</body>')
print('</html>')

s.close


