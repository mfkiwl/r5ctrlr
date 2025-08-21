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
import fnmatch

# constants


# defaults


# ----------  html header  ---------------------

print('Content-type:text/html\r\n\r\n')
print('<!DOCTYPE html>')
print('<html>')
print('<head>')
print('  <style>')
print('    .fraction {')
print('      display: inline-block;')
print('      text-align: center;')
print('    }')
print('')
print('    .fraction .numerator,')
print('    .fraction .denominator {')
print('      display: flex;')
print('      justify-content: center;')
print('      gap: 5px;')
print('    }')
print('')
print('    .fraction .line {')
print('      border-top: 2px solid black;')
print('      margin: 2px 0;')
print('    }')
print('')
print('    input[type="number"] {')
print('      width: 70px;')
print('      text-align: center;')
print('    }')
print('')
print('  </style>')
print('')
print('  <table>')
print('    <tr>')
print('      <td> <a href="/cgi-bin/index.cgi"> <img src="/MAX-IV_logo1_rgb-300x104.png" alt="MaxIV Laboratory"> </a> </td>')
print('      <td>')
print('      <H1>Max IV R5 controller</H1>')
print('      <H2>Real Time Controller Facility</H2>')
print('      </td>')
print('    </tr>')
print('  </table>')
print('</head>')

print('<body>')


# --------- open a connection to r5ctrlr SCPI server ----------
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 8888))
#s.connect(("192.168.0.18", 8888))


# ------------------  get current config from R5  --------------------


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
  if((stb&0x04) != 0):
    globalEnable=True
  else:
    globalEnable=False
else:
  globalEnable=False

# ----- get channel config

ch_conf=[dict() for ch in range(4)]

for ch in range(4):
  qstr='CTRLLOOP:CH:STATE? '+str(ch+1)+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  tok=ans.split(" ",2)
  if(tok[0].strip()=="OK:"):
    ch_conf[ch]['en']=tok[1].strip()
  else:
    ch_conf[ch]['en']='OFF'

  qstr='ADC:CH:OFFS? '+str(ch+1)+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  tok=ans.split(" ",2)
  if(tok[0].strip()=="OK:"):
    ch_conf[ch]['ADCoffs']=int(tok[1].strip())
  else:
    ch_conf[ch]['ADCoffs']=0
  
  qstr='ADC:CH:G? '+str(ch+1)+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  tok=ans.split(" ",2)
  if(tok[0].strip()=="OK:"):
    ch_conf[ch]['ADCgain']=float(tok[1].strip())
  else:
    ch_conf[ch]['ADCgain']=1.0
  
  qstr='DAC:CH:OFFS? '+str(ch+1)+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  tok=ans.split(" ",2)
  if(tok[0].strip()=="OK:"):
    ch_conf[ch]['DACoffs']=int(tok[1].strip())
  else:
    ch_conf[ch]['DACoffs']=0

  qstr='CTRLLOOP:CH:IN_SEL? '+str(ch+1)+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  tok=ans.split(" ",2)
  if(tok[0].strip()=="OK:"):
    ch_conf[ch]['cmdsel']=int(tok[1].strip())
  else:
    ch_conf[ch]['cmdsel']=(ch+1)

  qstr='DAC:CH:OUT_SEL? '+str(ch+1)+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  tok=ans.split(" ",2)
  if(tok[0].strip()=="OK:"):
    ch_conf[ch]['outsel']=int(tok[1].strip())
  else:
    ch_conf[ch]['outsel']=(ch+1)

  #------- MISO matrices

  for matname in ['A','B','C','D','E','F']:
    ch_conf[ch][matname]=[0. for matcol in range(5)]
    qstr='CTRLLOOP:MATRIXROW? '+matname+' '+str(ch+1)+'\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")
    tok=ans.split(" ",6)
    if(tok[0].strip()=="OK:"):
      for matcol in range(5):
        ch_conf[ch][matname][matcol]=float(tok[matcol+1].strip())
    else:
      # if I got an error, use defaults
      if((matname=='A') or (matname=='C') or (matname=='E')):
        ch_conf[ch][matname][ch+1]=1.0
      elif((matname=='B') or (matname=='D') or (matname=='F')):
        ch_conf[ch][matname][0]=1.0

  #------- PID instances configuration

  for pidinst in ['1','2']:
    qstr='CTRLLOOP:CH:PID:G? '+str(ch+1)+' '+ pidinst + '\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")
    tok=ans.split(" ",6)
    if(tok[0].strip()=="OK:"):
      ch_conf[ch]["PID"+pidinst+"_Gp"]   =float(tok[1].strip())
      ch_conf[ch]["PID"+pidinst+"_Gi"]   =float(tok[2].strip())
      ch_conf[ch]["PID"+pidinst+"_G1d"]  =float(tok[3].strip())
      ch_conf[ch]["PID"+pidinst+"_G2d"]  =float(tok[4].strip())
      ch_conf[ch]["PID"+pidinst+"_G_aiw"]=float(tok[5].strip())
    else:
      ch_conf[ch]["PID"+pidinst+"_Gp"]   = 1.0
      ch_conf[ch]["PID"+pidinst+"_Gi"]   = 0.0
      ch_conf[ch]["PID"+pidinst+"_G1d"]  = 0.0
      ch_conf[ch]["PID"+pidinst+"_G2d"]  = 0.0
      ch_conf[ch]["PID"+pidinst+"_G_aiw"]= 1.0

    # now calculate regular pID gains ki, kd from PID coefficients (kp=Gp)
    ch_conf[ch]["PID"+pidinst+"_ki"] = ch_conf[ch]["PID"+pidinst+"_Gi"]*2.*fsampl
    R=0.5*(1.+ch_conf[ch]["PID"+pidinst+"_G1d"])/(1.-ch_conf[ch]["PID"+pidinst+"_G1d"])
    Gd=(2.*R+1.)*ch_conf[ch]["PID"+pidinst+"_G2d"]
    ch_conf[ch]["PID"+pidinst+"_kd"] = Gd/(2.*fsampl)
    # de-warp f_filt
    fwarp=fsampl/R
    ch_conf[ch]["PID"+pidinst+"_ffilt"] = fsampl/np.pi*np.arctan(np.pi*fwarp/fsampl)
    
    
    qstr='CTRLLOOP:CH:PID:THR? '+str(ch+1)+' '+ pidinst + '\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")
    tok=ans.split(" ",3)
    if(tok[0].strip()=="OK:"):
      ch_conf[ch]["PID"+pidinst+"_in_thr"]  =float(tok[1].strip())
      ch_conf[ch]["PID"+pidinst+"_out_sat"] =float(tok[2].strip())
    else:
      ch_conf[ch]["PID"+pidinst+"_in_thr"]  = 0.0
      ch_conf[ch]["PID"+pidinst+"_out_sat"] = 1.0

    qstr='CTRLLOOP:CH:PID:INVERTCMD? '+str(ch+1)+' '+ pidinst + '\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")
    tok=ans.split(" ",2)
    if(tok[0].strip()=="OK:"):
      ch_conf[ch]["PID"+pidinst+"_invert_cmd"] = tok[1].strip()
    else:
      ch_conf[ch]["PID"+pidinst+"_invert_cmd"] = 'OFF'

    qstr='CTRLLOOP:CH:PID:INVERTMEAS? '+str(ch+1)+' '+ pidinst + '\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")
    tok=ans.split(" ",2)
    if(tok[0].strip()=="OK:"):
      ch_conf[ch]["PID"+pidinst+"_invert_meas"] = tok[1].strip()
    else:
      ch_conf[ch]["PID"+pidinst+"_invert_meas"] = 'OFF'

    qstr='CTRLLOOP:CH:PID:PVDERIV? '+str(ch+1)+' '+ pidinst + '\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")
    tok=ans.split(" ",2)
    if(tok[0].strip()=="OK:"):
      ch_conf[ch]["PID"+pidinst+"_PVderiv"] = tok[1].strip()
    else:
      ch_conf[ch]["PID"+pidinst+"_PVderiv"] = 'OFF'

  #------- IIR instances configuration
  
  for iirinst in ['1','2']:
    qstr='CTRLLOOP:CH:IIR:COEFF? '+str(ch+1)+' '+ iirinst + '\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")
    tok=ans.split(" ",6)
    if(tok[0].strip()=="OK:"):
      ch_conf[ch]["IIR"+iirinst+"_A0"] =float(tok[1].strip())
      ch_conf[ch]["IIR"+iirinst+"_A1"] =float(tok[2].strip())
      ch_conf[ch]["IIR"+iirinst+"_A2"] =float(tok[3].strip())
      ch_conf[ch]["IIR"+iirinst+"_B1"] =float(tok[4].strip())
      ch_conf[ch]["IIR"+iirinst+"_B2"] =float(tok[5].strip())
    else:
      ch_conf[ch]["IIR"+iirinst+"_A0"] = 1.0
      ch_conf[ch]["IIR"+iirinst+"_A1"] = 0.0
      ch_conf[ch]["IIR"+iirinst+"_A2"] = 0.0
      ch_conf[ch]["IIR"+iirinst+"_B1"] = 0.0
      ch_conf[ch]["IIR"+iirinst+"_B2"] = 0.0


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
ch_to_update=0

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
  elif name=='globenbl':
    if int(arguments[name][0])==1:
      cmd_s='CTRLLOOP ON\n'
      globalEnable=True
    else:
      cmd_s='CTRLLOOP OFF\n'
      globalEnable=False
    s.sendall(cmd_s.encode('ascii')) 
    ans=(s.recv(1024)).decode('utf-8')
    tok=ans.split(" ",2)
    if tok[0].strip()=='ERR:':
      print('<br>Error commanding the global ON/OFF state<br>')

  # -------- now check channel enable request; channel number is a suffix;
  # -------- I use fnmatch to test with wildcard

  # --------  channel to update (only one at a time) ----------
  elif fnmatch.fnmatch(name,'updconf?'):
    ch=int(name[-1])
    if int(arguments[name][0])==1:
      ch_to_update=ch

  # --------  channel enable  ----------
  elif fnmatch.fnmatch(name,'ch_enbl?'):
    ch=int(name[-1])-1
    if int(arguments[name][0])==1:
      ch_conf[ch]['en']='ON'
    else:
      ch_conf[ch]['en']='OFF'

  # --------  ADC offs  ----------
  elif fnmatch.fnmatch(name,'adc_offs?'):
    ch=int(name[-1])-1
    ch_conf[ch]['ADCoffs']=int(arguments[name][0])

  # --------  ADC gain  ----------
  elif fnmatch.fnmatch(name,'adc_gain?'):
    ch=int(name[-1])-1
    ch_conf[ch]['ADCgain']=float(arguments[name][0])

  # --------  MISO matrix  ----------
  elif fnmatch.fnmatch(name,'[abcdef][1234][01234]'):
    matcol=int(name[-1])
    ch=int(name[-2])-1
    matname=str(name[0]).upper()
    ch_conf[ch][matname][matcol]=float(arguments[name][0])
    #print(f'{matname}{ch+1}{matcol}={float(arguments[name][0])}<br>')

  # --------  cmd input selector  ----------
  elif fnmatch.fnmatch(name,'cmd_select?'):
    ch=int(name[-1])-1
    ch_conf[ch]['cmdsel']=int(arguments[name][0])

  # --------  DAC out selector  ----------
  elif fnmatch.fnmatch(name,'dac_select?'):
    ch=int(name[-1])-1
    ch_conf[ch]['outsel']=int(arguments[name][0])

  # --------  DAC offs  ----------
  elif fnmatch.fnmatch(name,'dac_offs?'):
    ch=int(name[-1])-1
    ch_conf[ch]['DACoffs']=int(arguments[name][0])

  # --------  PID kp  ----------
  elif fnmatch.fnmatch(name,'pid[12]_kp_ch[1234]'):
    pidinst=name[3]
    ch=int(name[-1])-1
    ch_conf[ch]["PID"+pidinst+"_Gp"]=float(arguments[name][0])

  # --------  PID ki  ----------
  elif fnmatch.fnmatch(name,'pid[12]_ki_ch[1234]'):
    pidinst=name[3]
    ch=int(name[-1])-1
    ch_conf[ch]["PID"+pidinst+"_ki"]=float(arguments[name][0])

  # --------  PID kd  ----------
  elif fnmatch.fnmatch(name,'pid[12]_kd_ch[1234]'):
    pidinst=name[3]
    ch=int(name[-1])-1
    ch_conf[ch]["PID"+pidinst+"_kd"]=float(arguments[name][0])

  # --------  PID F_filt  ----------
  elif fnmatch.fnmatch(name,'pid[12]_ffilt_ch[1234]'):
    pidinst=name[3]
    ch=int(name[-1])-1
    ch_conf[ch]["PID"+pidinst+"_ffilt"]=float(arguments[name][0])

  # --------  PID G_aiw  ----------
  elif fnmatch.fnmatch(name,'pid[12]_g_aiw_ch[1234]'):
    pidinst=name[3]
    ch=int(name[-1])-1
    ch_conf[ch]["PID"+pidinst+"_G_aiw"]=float(arguments[name][0])

  # --------  PID dead band  ----------
  elif fnmatch.fnmatch(name,'pid[12]_in_thr_ch[1234]'):
    pidinst=name[3]
    ch=int(name[-1])-1
    ch_conf[ch]["PID"+pidinst+"_in_thr"]=float(arguments[name][0])

  # --------  PID saturation limit  ----------
  elif fnmatch.fnmatch(name,'pid[12]_out_sat_ch[1234]'):
    pidinst=name[3]
    ch=int(name[-1])-1
    ch_conf[ch]["PID"+pidinst+"_out_sat"]=float(arguments[name][0])

  # --------  PID derivative on process variable  ----------
  elif fnmatch.fnmatch(name,'pid[12]_PVderiv_ch[1234]'):
    pidinst=name[3]
    ch=int(name[-1])-1
    if int(arguments[name][0])==1:
      ch_conf[ch]["PID"+pidinst+"_PVderiv"]='ON'
    else:
      ch_conf[ch]["PID"+pidinst+"_PVderiv"]='OFF'

  # --------  PID invert cmd  ----------
  elif fnmatch.fnmatch(name,'pid[12]_invcmd_ch[1234]'):
    pidinst=name[3]
    ch=int(name[-1])-1
    if int(arguments[name][0])==1:
      ch_conf[ch]["PID"+pidinst+"_invert_cmd"]='ON'
    else:
      ch_conf[ch]["PID"+pidinst+"_invert_cmd"]='OFF'

  # --------  PID invert meas  ----------
  elif fnmatch.fnmatch(name,'pid[12]_invmeas_ch[1234]'):
    pidinst=name[3]
    ch=int(name[-1])-1
    if int(arguments[name][0])==1:
      ch_conf[ch]["PID"+pidinst+"_invert_meas"]='ON'
    else:
      ch_conf[ch]["PID"+pidinst+"_invert_meas"]='OFF'

  # --------  IIR coefficients  ----------
  elif fnmatch.fnmatch(name,'iir[12]_[ab][012]_ch[1234]'):
    iirinst=name[3]
    matname=str(name[5]).upper()
    matindx=name[6]
    ch=int(name[-1])-1
    ch_conf[ch]["IIR"+iirinst+"_"+matname+matindx] =float(arguments[name][0])

  # --------  PID reset  ----------
  elif fnmatch.fnmatch(name,'resetPID[12]ch[1234]'):
    pidinst=name[8]
    pidch=name[-1]
    qstr='CTRLLOOP:CH:PID:RESET '+pidch+' '+pidinst+'\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")

  # --------  IIR reset  ----------
  elif fnmatch.fnmatch(name,'resetIIR[12]ch[1234]'):
    iirinst=name[8]
    iirch=name[-1]
    qstr='CTRLLOOP:CH:IIR:RESET '+iirch+' '+iirinst+'\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")



# ---------       now workout the low-level coefficients          ------------
# ---------  and write back the updated channel config to the R5  ------------

if( (ch_to_update>=1) and (ch_to_update<=4) ):
  ch=ch_to_update-1

  qstr='CTRLLOOP:CH:STATE '+str(ch+1)+' '                \
                           +ch_conf[ch]['en']+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  #tok=ans.split(" ",2)
  #if(tok[0].strip()=="OK:"):
  #  print(f'updated ch {ch+1}\n')
  #else:
  #  print(f'error updating ch {ch+1}\n')

  qstr='ADC:CH:OFFS '+str(ch+1)+' '                \
                     +str(ch_conf[ch]['ADCoffs'])+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")

  qstr='ADC:CH:G '+str(ch+1)+' '                \
                  +str(ch_conf[ch]['ADCgain'])+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")
  
  qstr='CTRLLOOP:CH:IN_SEL '+str(ch+1)+' '                \
                            +str(ch_conf[ch]['cmdsel'])+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")

  for matname in ['A','B','C','D','E','F']:
    qstr='CTRLLOOP:MATRIXROW '+matname+' '+str(ch+1)+' '           \
                             +str(ch_conf[ch][matname][0])+' ' \
                             +str(ch_conf[ch][matname][1])+' ' \
                             +str(ch_conf[ch][matname][2])+' ' \
                             +str(ch_conf[ch][matname][3])+' ' \
                             +str(ch_conf[ch][matname][4])+'\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")

  qstr='DAC:CH:OFFS '+str(ch+1)+' '                \
                     +str(ch_conf[ch]['DACoffs'])+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")

  qstr='DAC:CH:OUT_SEL '+str(ch+1)+' '                \
                        +str(ch_conf[ch]['outsel'])+'\n'
  s.sendall(bytes(qstr,encoding='ascii')) 
  ans=(s.recv(1024)).decode("utf-8")


  for iirinst in ["1","2"]:
    qstr='CTRLLOOP:CH:IIR:COEFF '+str(ch+1)+' '+iirinst+' '                     \
                                 +str(ch_conf[ch]["IIR"+iirinst+"_A0"])+' ' \
                                 +str(ch_conf[ch]["IIR"+iirinst+"_A1"])+' ' \
                                 +str(ch_conf[ch]["IIR"+iirinst+"_A2"])+' ' \
                                 +str(ch_conf[ch]["IIR"+iirinst+"_B1"])+' ' \
                                 +str(ch_conf[ch]["IIR"+iirinst+"_B2"])+'\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")


  for pidinst in ["1","2"]:
    # calculate Gi
    ch_conf[ch]["PID"+pidinst+"_Gi"]=ch_conf[ch]["PID"+pidinst+"_ki"]/(2.*fsampl)
    # calculate G1D, G2D
    fwarp=fsampl/np.pi*np.tan(np.pi*ch_conf[ch]["PID"+pidinst+"_ffilt"]/fsampl)
    R=fsampl/fwarp
    Gd=ch_conf[ch]["PID"+pidinst+"_kd"]*2.*fsampl
    ch_conf[ch]["PID"+pidinst+"_G1d"]=(2.*R-1)/(2.*R+1)
    ch_conf[ch]["PID"+pidinst+"_G2d"]=Gd/(2.*R+1)

    qstr='CTRLLOOP:CH:PID:G '+str(ch+1)+' '+ pidinst + ' '                 \
                             +str(ch_conf[ch]["PID"+pidinst+"_Gp"])+' '    \
                             +str(ch_conf[ch]["PID"+pidinst+"_Gi"])+' '    \
                             +str(ch_conf[ch]["PID"+pidinst+"_G1d"])+' '   \
                             +str(ch_conf[ch]["PID"+pidinst+"_G2d"])+' '   \
                             +str(ch_conf[ch]["PID"+pidinst+"_G_aiw"])+'\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")

    qstr='CTRLLOOP:CH:PID:THR '+str(ch+1)+' '+ pidinst + ' '                   \
                               +str(ch_conf[ch]["PID"+pidinst+"_in_thr"])+' '  \
                               +str(ch_conf[ch]["PID"+pidinst+"_out_sat"])+'\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")

    qstr='CTRLLOOP:CH:PID:INVERTCMD '+str(ch+1)+' '+ pidinst + ' '                 \
                                     +ch_conf[ch]["PID"+pidinst+"_invert_cmd"]+'\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")

    qstr='CTRLLOOP:CH:PID:INVERTMEAS '+str(ch+1)+' '+ pidinst + ' '                  \
                                      +ch_conf[ch]["PID"+pidinst+"_invert_meas"]+'\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")

    qstr='CTRLLOOP:CH:PID:PVDERIV '+str(ch+1)+' '+ pidinst + ' '              \
                                   +ch_conf[ch]["PID"+pidinst+"_PVderiv"]+'\n'
    s.sendall(bytes(qstr,encoding='ascii')) 
    ans=(s.recv(1024)).decode("utf-8")



# --------------------  now display html page  -----------------------


print('<img src="/ctrlloop.png" width="800" alt="Control loop block diagram">')

print('<form action="" method="GET" >')

# -- Prevent implicit submission of the form hitting <enter> key
print('  <button type="submit" disabled style="display: none" aria-hidden="true"></button>')


print('  <table>')

#-------  Fsampl  -----------------------------
print('    <tr>')
print('      <td>Sampling Frequency:</td>')
print('      <td>')
print(f'          <input type="number" name="fsampl" id="fsampl" value="{fsampl}" min="1" max="10000" step="1" onchange="javascript:this.form.submit()"> Hz')
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

print('  </table>')
print('</form>')

#-------  channel conf  -----------------------------

for ch in range(4):
  print('<form action="" method="GET" >')
  # -- Prevent implicit submission of the form hitting <enter> key
  print('<button type="submit" disabled style="display: none" aria-hidden="true"></button>')
  print('  <table>')
  #-------  spacer  -----------------------------
  print('  <tr><td colspan="2" class="divider"><hr /></td></tr>')
  #-------  channel number  -----------------------------
  print(f'  <tr><td colspan="2"><h3>Control Loop Channel {ch+1} Configuration</h3></td></tr>')
  
  #-------  channel enable  -----------------------------
  print('    <tr>')
  print('      <td>State:</td>')
  print('      <td>')
  print(f'        <select name="ch_enbl{ch+1}" id = "ch_enbl{ch+1}">')
  if ch_conf[ch]['en']=="ON" :
    print('          <option value = "0"         >OFF</option>')
    print('          <option value = "1" selected>ON</option>')
  else:
    print('          <option value = "0" selected>OFF</option>')
    print('          <option value = "1"         >ON</option>')
  print('        </select>')
  print('      </td>')
  print('    </tr>')

  print('    <tr><td>&nbsp</td></tr>')

  #-------  ADC offset  -----------------------------
  print('    <tr>')
  print(f'      <td>ADC#{ch+1} offset =</td>')
  print('      <td>')
  # offset max is 32768 and not 32767 because we may want to have ADC range=[0,65535] instead of [-32768,32767]
  print(f'          <input type="number" name="adc_offs{ch+1}" id="adc_offs{ch+1}" value="{ch_conf[ch]["ADCoffs"]}" min="-32768" max="32768" step="1"> counts')
  print('      </td>')
  print('    </tr>')

  #-------  ADC gain  -----------------------------
  print('    <tr>')
  print(f'      <td>ADC#{ch+1} gain =</td>')
  print('      <td>')
  print(f'          <input type="number" name="adc_gain{ch+1}" id="adc_gain{ch+1}" step="any" value="{ch_conf[ch]["ADCgain"]}">')
  print('      </td>')
  print('    </tr>')

  print('    <tr><td>&nbsp</td></tr>')

  #-------  A,B MISO matrix for measurement input  -----------------------------
  #print('    <tr><td colspan="2">MEAS input matrix:</td></tr>')
  print('    <tr>')
  print('      <td>')
  print(f'     CH{ch+1}<sub>meas</sub> = ')
  print('      </td>')
  print('      <td>')
  print('       <div class="fraction">')
  print('         <!-- Numerator -->')
  print('         <div class="numerator">')
  print(f'           <input type="number" name="a{ch+1}0" placeholder="a{ch+1}0" step="any" value="{ch_conf[ch]["A"][0]}"> +')
  print(f'           <input type="number" name="a{ch+1}1" placeholder="a{ch+1}1" step="any" value="{ch_conf[ch]["A"][1]}"> &bull; ADC1 +')
  print(f'           <input type="number" name="a{ch+1}2" placeholder="a{ch+1}2" step="any" value="{ch_conf[ch]["A"][2]}"> &bull; ADC2 +')
  print(f'           <input type="number" name="a{ch+1}3" placeholder="a{ch+1}3" step="any" value="{ch_conf[ch]["A"][3]}"> &bull; ADC3 +')
  print(f'           <input type="number" name="a{ch+1}4" placeholder="a{ch+1}4" step="any" value="{ch_conf[ch]["A"][4]}"> &bull; ADC4 ')
  print('         </div>')
  print('         <!-- Fraction line -->')
  print('         <div class="line"></div>')
  print('         <!-- Denominator -->')
  print('         <div class="denominator">')
  print(f'           <input type="number" name="b{ch+1}0" placeholder="b{ch+1}0" step="any" value="{ch_conf[ch]["B"][0]}"> +')
  print(f'           <input type="number" name="b{ch+1}1" placeholder="b{ch+1}1" step="any" value="{ch_conf[ch]["B"][1]}"> &bull; ADC1 +')
  print(f'           <input type="number" name="b{ch+1}2" placeholder="b{ch+1}2" step="any" value="{ch_conf[ch]["B"][2]}"> &bull; ADC2 +')
  print(f'           <input type="number" name="b{ch+1}3" placeholder="b{ch+1}3" step="any" value="{ch_conf[ch]["B"][3]}"> &bull; ADC3 +')
  print(f'           <input type="number" name="b{ch+1}4" placeholder="b{ch+1}4" step="any" value="{ch_conf[ch]["B"][4]}"> &bull; ADC4')
  print('         </div>')
  print('       </div>')
  print('       </td>')
  print('     </tr>')
  
  print('    <tr><td>&nbsp</td></tr>')

  #-------  C,D MISO matrix for command input  -----------------------------
  #print('    <tr><td colspan="2">CMD input matrix:</td></tr>')
  print('    <tr>')
  print('      <td>')
  print(f'     CH{ch+1}<sub>cmd</sub> = ')
  print('      </td>')
  print('      <td>')
  print('       <div class="fraction">')
  print('         <!-- Numerator -->')
  print('         <div class="numerator">')
  print(f'           <input type="number" name="c{ch+1}0" placeholder="c{ch+1}0" step="any" value="{ch_conf[ch]["C"][0]}"> +')
  print(f'           <input type="number" name="c{ch+1}1" placeholder="c{ch+1}1" step="any" value="{ch_conf[ch]["C"][1]}"> &bull; ADC1 +')
  print(f'           <input type="number" name="c{ch+1}2" placeholder="c{ch+1}2" step="any" value="{ch_conf[ch]["C"][2]}"> &bull; ADC2 +')
  print(f'           <input type="number" name="c{ch+1}3" placeholder="c{ch+1}3" step="any" value="{ch_conf[ch]["C"][3]}"> &bull; ADC3 +')
  print(f'           <input type="number" name="c{ch+1}4" placeholder="c{ch+1}4" step="any" value="{ch_conf[ch]["C"][4]}"> &bull; ADC4 ')
  print('         </div>')
  print('         <!-- Fraction line -->')
  print('         <div class="line"></div>')
  print('         <!-- Denominator -->')
  print('         <div class="denominator">')
  print(f'           <input type="number" name="d{ch+1}0" placeholder="d{ch+1}0" step="any" value="{ch_conf[ch]["D"][0]}"> +')
  print(f'           <input type="number" name="d{ch+1}1" placeholder="d{ch+1}1" step="any" value="{ch_conf[ch]["D"][1]}"> &bull; ADC1 +')
  print(f'           <input type="number" name="d{ch+1}2" placeholder="d{ch+1}2" step="any" value="{ch_conf[ch]["D"][2]}"> &bull; ADC2 +')
  print(f'           <input type="number" name="d{ch+1}3" placeholder="d{ch+1}3" step="any" value="{ch_conf[ch]["D"][3]}"> &bull; ADC3 +')
  print(f'           <input type="number" name="d{ch+1}4" placeholder="d{ch+1}4" step="any" value="{ch_conf[ch]["D"][4]}"> &bull; ADC4')
  print('         </div>')
  print('       </div>')
  print('       </td>')
  print('     </tr>')

  print('    <tr><td>&nbsp</td></tr>')


  #-------  2 PID instances  -----------------------------
  
  for pidinst in ['1','2']:
    print(f'    <tr><td colspan="2">PID#{pidinst}</td></tr>')
    
    if(pidinst=='1'):
      #-------  cmd input selector  -----------------------------
      print('    <tr>')
      print('      <td>cmd input:</td>')
      print('      <td>')
      print(f'        <select name="cmd_select{ch+1}" id = "cmd_select{ch+1}">')
      for selopt in range(4):
        print(f'          <option value = "{selopt+1}" ')
        if ch_conf[ch]['cmdsel']==(selopt+1):
          print('selected')
        print(f' >waveform generator ch#{selopt+1}</option>')
      print('          <option value = "5" ')
      if ch_conf[ch]['cmdsel']==5:
        print('selected')
      print(f' >CH{ch+1}<sub>cmd</sub> MISO matrix</option>')
      print('        </select>')
      print('      </td>')
      print('    </tr>')

    #-------  PID kp -----------------------------
    print('    <tr>')
    print(f'      <td>G<sub>prop</sub> = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_kp_ch{ch+1}" id="pid{pidinst}_kp_ch{ch+1}" step="any" value="{ch_conf[ch]["PID"+pidinst+"_Gp"]}">')
    print('      </td>')
    print('    </tr>')
  
    #-------  PID ki -----------------------------
    print('    <tr>')
    print(f'      <td>G<sub>integr</sub> = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_ki_ch{ch+1}" id="pid{pidinst}_ki_ch{ch+1}" step="any" value="{ch_conf[ch]["PID"+pidinst+"_ki"]}">')
    print('      </td>')
    print('    </tr>')
  
    #-------  PID kd -----------------------------
    print('    <tr>')
    print(f'      <td>G<sub>deriv</sub> = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_kd_ch{ch+1}" id="pid{pidinst}_kd_ch{ch+1}" step="any" value="{ch_conf[ch]["PID"+pidinst+"_kd"]}">')
    print('      </td>')
    print('    </tr>')
  
    #-------  PID F_filter (deriv term) -----------------------------
    print('    <tr>')
    print(f'      <td>F_filter<sub>deriv</sub> = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_ffilt_ch{ch+1}" id="pid{pidinst}_ffilt_ch{ch+1}" step="any" value="{ch_conf[ch]["PID"+pidinst+"_ffilt"]}"> Hz')
    print('      </td>')
    print('    </tr>')

    #-------  PID G_AIW -----------------------------
    print('    <tr>')
    print(f'      <td>G<sub>int.windup</sub> = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_g_aiw_ch{ch+1}" id="pid{pidinst}_g_aiw_ch{ch+1}" step="any" value="{ch_conf[ch]["PID"+pidinst+"_G_aiw"]}"> (it should be 1)')
    print('      </td>')
    print('    </tr>')
  
    #-------  PID input dead band -----------------------------
    print('    <tr>')
    print(f'      <td>in deadband = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_in_thr_ch{ch+1}" id="pid{pidinst}_in_thr_ch{ch+1}" step="any" value="{ch_conf[ch]["PID"+pidinst+"_in_thr"]}"> (fullscale = &plusmn;1.0)')
    print('      </td>')
    print('    </tr>')
  
    #-------  PID output saturation -----------------------------
    print('    <tr>')
    print(f'      <td>out saturation = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_out_sat_ch{ch+1}" id="pid{pidinst}_out_sat_ch{ch+1}" step="any" value="{ch_conf[ch]["PID"+pidinst+"_out_sat"]}"> (fullscale = &plusmn;1.0)')
    print('      </td>')
    print('    </tr>')
  
    #-------  extra options for PID#1 only  -----------------------------
    if(pidinst=='1'):

    #-------  PID derivative on PV  -----------------------------
      print('    <tr>')
      print(f'      <td>proc.var deriv:</td>')
      print('      <td>')
      print(f'        <select name="pid{pidinst}_PVderiv_ch{ch+1}" id = "pid{pidinst}_PVderiv_ch{ch+1}">')
      if ch_conf[ch]["PID"+pidinst+"_PVderiv"]=="ON" :
        print('          <option value = "0"         >OFF</option>')
        print('          <option value = "1" selected>ON</option>')
      else:
        print('          <option value = "0" selected>OFF</option>')
        print('          <option value = "1"         >ON</option>')
      print('        </select>')
      print('      </td>')
      print('    </tr>')
  
    #-------  invert cmd  -----------------------------
      print('    <tr>')
      print(f'      <td>invert cmd:</td>')
      print('      <td>')
      print(f'        <select name="pid{pidinst}_invcmd_ch{ch+1}" id = "pid{pidinst}_invcmd_ch{ch+1}">')
      if ch_conf[ch]["PID"+pidinst+"_invert_cmd"]=="ON" :
        print('          <option value = "0"         >OFF</option>')
        print('          <option value = "1" selected>ON</option>')
      else:
        print('          <option value = "0" selected>OFF</option>')
        print('          <option value = "1"         >ON</option>')
      print('        </select>')
      print('      </td>')
      print('    </tr>')
  
    #-------  invert meas  -----------------------------
      print('    <tr>')
      print(f'      <td>invert meas:</td>')
      print('      <td>')
      print(f'        <select name="pid{pidinst}_invmeas_ch{ch+1}" id = "pid{pidinst}_invmeas_ch{ch+1}">')
      if ch_conf[ch]["PID"+pidinst+"_invert_meas"]=="ON" :
        print('          <option value = "0"         >OFF</option>')
        print('          <option value = "1" selected>ON</option>')
      else:
        print('          <option value = "0" selected>OFF</option>')
        print('          <option value = "1"         >ON</option>')
      print('        </select>')
      print('      </td>')
      print('    </tr>')
  
  
      print('    <tr><td>&nbsp</td></tr>')
    
      #-------  2 IIR instances  nested inside the two PIDs-----------------------------
      for IIRinst in ['1','2']:  
        print('    <tr>')
        print(f'      <td>IIR#{IIRinst}: Y(n)=</td>')
        print('      <td>')
        print('        <div class="fraction">')
        print('          <div class="numerator">')
        print(f'            <input type="number" name="iir{IIRinst}_a0_ch{ch+1}" placeholder="a0" step="any" value="{ch_conf[ch]["IIR"+IIRinst+"_A0"]}"> &bull; X(n) +')
        print(f'            <input type="number" name="iir{IIRinst}_a1_ch{ch+1}" placeholder="a1" step="any" value="{ch_conf[ch]["IIR"+IIRinst+"_A1"]}"> &bull; X(n-1) +')
        print(f'            <input type="number" name="iir{IIRinst}_a2_ch{ch+1}" placeholder="a2" step="any" value="{ch_conf[ch]["IIR"+IIRinst+"_A2"]}"> &bull; X(n-2) -')
        print(f'            <input type="number" name="iir{IIRinst}_b1_ch{ch+1}" placeholder="b1" step="any" value="{ch_conf[ch]["IIR"+IIRinst+"_B1"]}"> &bull; Y(n-1) -')
        print(f'            <input type="number" name="iir{IIRinst}_b2_ch{ch+1}" placeholder="b2" step="any" value="{ch_conf[ch]["IIR"+IIRinst+"_B2"]}"> &bull; Y(n-2)')
        print('          </div>')
        print('        </div>')
        print('      </td>')
        print('    </tr>')
        print('    <tr><td>&nbsp</td></tr>')

      print('<tr><td colspan="2">')
      print('Please remember that <a href="/iircoeff.html">here</a> you can find an utility ')
      print('to calcultate the IIR coefficients for common cases')
      print('</td></tr>')
      print('<tr><td>&nbsp</td></tr>')


  print('    <tr><td>&nbsp</td></tr>')

  #-------  E,F output MISO matrix -----------------------------
  
  #print('    <tr><td colspan="2">output matrix:</td></tr>')
  print('    <tr>')
  print('      <td>')
  print(f'     DAC#{ch+1}<sub>cmd</sub> = ')
  print('      </td>')
  print('      <td>')
  print('       <div class="fraction">')
  print('         <!-- Numerator -->')
  print('         <div class="numerator">')
  print(f'           <input type="number" name="e{ch+1}0" placeholder="e{ch+1}0" step="any" value="{ch_conf[ch]["E"][0]}"> +')
  print(f'           <input type="number" name="e{ch+1}1" placeholder="e{ch+1}1" step="any" value="{ch_conf[ch]["E"][1]}"> &bull; CH1 +')
  print(f'           <input type="number" name="e{ch+1}2" placeholder="e{ch+1}2" step="any" value="{ch_conf[ch]["E"][2]}"> &bull; CH2 +')
  print(f'           <input type="number" name="e{ch+1}3" placeholder="e{ch+1}3" step="any" value="{ch_conf[ch]["E"][3]}"> &bull; CH3 +')
  print(f'           <input type="number" name="e{ch+1}4" placeholder="e{ch+1}4" step="any" value="{ch_conf[ch]["E"][4]}"> &bull; CH4 ')
  print('         </div>')
  print('         <!-- Fraction line -->')
  print('         <div class="line"></div>')
  print('         <!-- Denominator -->')
  print('         <div class="denominator">')
  print(f'           <input type="number" name="f{ch+1}0" placeholder="f{ch+1}0" step="any" value="{ch_conf[ch]["F"][0]}"> +')
  print(f'           <input type="number" name="f{ch+1}1" placeholder="f{ch+1}1" step="any" value="{ch_conf[ch]["F"][1]}"> &bull; CH1 +')
  print(f'           <input type="number" name="f{ch+1}2" placeholder="f{ch+1}2" step="any" value="{ch_conf[ch]["F"][2]}"> &bull; CH2 +')
  print(f'           <input type="number" name="f{ch+1}3" placeholder="f{ch+1}3" step="any" value="{ch_conf[ch]["F"][3]}"> &bull; CH3 +')
  print(f'           <input type="number" name="f{ch+1}4" placeholder="f{ch+1}4" step="any" value="{ch_conf[ch]["F"][4]}"> &bull; CH4')
  print('         </div>')
  print('       </div>')
  print('       </td>')
  print('     </tr>')

  print('    <tr><td>&nbsp</td></tr>')

  #-------  DAC out selector  -----------------------------
  print('    <tr>')
  print(f'      <td>DAC#{ch+1} driver:</td>')
  print('      <td>')
  print(f'        <select name="dac_select{ch+1}" id = "dac_select{ch+1}">')
  for selopt in range(4):
    print(f'          <option value = "{selopt+1}" ')
    if ch_conf[ch]['outsel']==(selopt+1):
      print('selected')
    print(f' >waveform generator ch#{selopt+1}</option>')
  print('          <option value = "5" ')
  if ch_conf[ch]['outsel']==5:
    print('selected')
  print(f' >DAC#{ch+1}<sub>cmd</sub> MISO matrix</option>')
  print('        </select>')
  print('      </td>')
  print('    </tr>')

  #-------  DAC offset  -----------------------------
  print('    <tr>')
  print(f'      <td>DAC#{ch+1} offset =</td>')
  print('      <td>')
  # offset max is 32768 to mimic the ADC case
  print(f'          <input type="number" name="dac_offs{ch+1}" id="dac_offs{ch+1}" step="1" value="{ch_conf[ch]["DACoffs"]}" min="-32768" max="32768" step=1> counts')
  print('      </td>')
  print('    </tr>')

  print('    <tr><td>&nbsp</td></tr>')

  #-------  Submit button  -----------------------------
  print('    <tr><td colspan="2">')
  print(f'     <button type="submit" name="updconf{ch+1}" id="updconf{ch+1}" value=1> update CH#{ch+1} configuration</button>')
  print('    </td></tr>')

  print('</form>')

  #-------  PID and IIR Reset buttons  -----------------------------
  print('    <tr>')
  print('      <td colspan="2">')

  print('      <table><tr>')
  print('      <td>')
  print('        <form>')
  print(f'       <button type="submit" name="resetPID1ch{ch+1}" id="resetPID1ch{ch+1}" value=1>Reset PID#1</button>')
  print('        </form>')
  print('      </td>')
  print('      <td>')
  print('        <form>')
  print(f'       <button type="submit" name="resetIIR1ch{ch+1}" id="resetIIR1ch{ch+1}" value=1>Reset IIR#1</button>')
  print('        </form>')
  print('      </td>')
  print('      <td>')
  print('        <form>')
  print(f'       <button type="submit" name="resetIIR2ch{ch+1}" id="resetIIR2ch{ch+1}" value=1>Reset IIR#2</button>')
  print('        </form>')
  print('      </td>')
  print('      <td>')
  print('        <form>')
  print(f'       <button type="submit" name="resetPID2ch{ch+1}" id="resetPID2ch{ch+1}" value=1>Reset PID#2</button>')
  print('        </form>')
  print('      </td>')
  print('      </tr></table>')
  
  print('      </td>')
  print('    </tr>')


  #-------  last spacer  -----------------------------
  if(ch==3):
    print('    <tr><td colspan="2" class="divider"><hr /></td></tr>')
  print('  </table>')

  #print('</form>')

print('<br><br>')
print('</body>')
print('</html>')

s.close


