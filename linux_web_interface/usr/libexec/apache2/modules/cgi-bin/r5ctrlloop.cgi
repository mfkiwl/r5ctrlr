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
print('      <td> <a href="/index.html"> <img src="/MAX-IV_logo1_rgb-300x104.png" alt="MaxIV Laboratory"> </a> </td>')
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
#s.connect(("127.0.0.1", 8888))
s.connect(("192.168.0.18", 8888))

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
    ch_conf[ch]['outsel']=tok[1].strip()
  else:
    ch_conf[ch]['outsel']='WAVEGEN'

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


# --------------------  now display html page  -----------------------

print('<img src="/ctrlloop.png" alt="Control loop block diagram">')

print('<form action="" method="GET" >')

# -- Prevent implicit submission of the form hitting <enter> key
print('  <button type="submit" disabled style="display: none" aria-hidden="true"></button>')


print('  <table>')

#-------  Fsampl  -----------------------------
print('    <tr>')
print('      <td>Sampling Frequency:</td>')
print('      <td>')
print(f'          <input type="number" name="fsampl" id="fsampl" value="{fsampl}" min="1" max="10000" step=1 onchange="javascript:this.form.submit()"> Hz')
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
  print(f'          <input type="number" name="adc_offs{ch+1}" id="adc_offs{ch+1}" value="{ch_conf[ch]["ADCoffs"]}" min="-32768" max="32767" step=1> counts')
  print('      </td>')
  print('    </tr>')

  #-------  ADC gain  -----------------------------
  print('    <tr>')
  print(f'      <td>ADC#{ch+1} gain =</td>')
  print('      <td>')
  print(f'          <input type="number" name="adc_gain{ch+1}" id="adc_gain{ch+1}" value="{ch_conf[ch]["ADCgain"]}">')
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
  print(f'           <input type="number" name="a{ch+1}0" placeholder="a{ch+1}0" value="{ch_conf[ch]["A"][0]}"> +')
  print(f'           <input type="number" name="a{ch+1}1" placeholder="a{ch+1}1" value="{ch_conf[ch]["A"][1]}"> &bull; ADC1 +')
  print(f'           <input type="number" name="a{ch+1}2" placeholder="a{ch+1}2" value="{ch_conf[ch]["A"][2]}"> &bull; ADC2 +')
  print(f'           <input type="number" name="a{ch+1}3" placeholder="a{ch+1}3" value="{ch_conf[ch]["A"][3]}"> &bull; ADC3 +')
  print(f'           <input type="number" name="a{ch+1}4" placeholder="a{ch+1}4" value="{ch_conf[ch]["A"][4]}"> &bull; ADC4 ')
  print('         </div>')
  print('         <!-- Fraction line -->')
  print('         <div class="line"></div>')
  print('         <!-- Denominator -->')
  print('         <div class="denominator">')
  print(f'           <input type="number" name="b{ch+1}0" placeholder="b{ch+1}0" value="{ch_conf[ch]["B"][0]}"> +')
  print(f'           <input type="number" name="b{ch+1}1" placeholder="b{ch+1}1" value="{ch_conf[ch]["B"][1]}"> &bull; ADC1 +')
  print(f'           <input type="number" name="b{ch+1}2" placeholder="b{ch+1}2" value="{ch_conf[ch]["B"][2]}"> &bull; ADC2 +')
  print(f'           <input type="number" name="b{ch+1}3" placeholder="b{ch+1}3" value="{ch_conf[ch]["B"][3]}"> &bull; ADC3 +')
  print(f'           <input type="number" name="b{ch+1}4" placeholder="b{ch+1}4" value="{ch_conf[ch]["B"][4]}"> &bull; ADC4')
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
  print(f'           <input type="number" name="c{ch+1}0" placeholder="c{ch+1}0" value="{ch_conf[ch]["C"][0]}"> +')
  print(f'           <input type="number" name="c{ch+1}1" placeholder="c{ch+1}1" value="{ch_conf[ch]["C"][1]}"> &bull; ADC1 +')
  print(f'           <input type="number" name="c{ch+1}2" placeholder="c{ch+1}2" value="{ch_conf[ch]["C"][2]}"> &bull; ADC2 +')
  print(f'           <input type="number" name="c{ch+1}3" placeholder="c{ch+1}3" value="{ch_conf[ch]["C"][3]}"> &bull; ADC3 +')
  print(f'           <input type="number" name="c{ch+1}4" placeholder="c{ch+1}4" value="{ch_conf[ch]["C"][4]}"> &bull; ADC4 ')
  print('         </div>')
  print('         <!-- Fraction line -->')
  print('         <div class="line"></div>')
  print('         <!-- Denominator -->')
  print('         <div class="denominator">')
  print(f'           <input type="number" name="d{ch+1}0" placeholder="d{ch+1}0" value="{ch_conf[ch]["D"][0]}"> +')
  print(f'           <input type="number" name="d{ch+1}1" placeholder="d{ch+1}1" value="{ch_conf[ch]["D"][1]}"> &bull; ADC1 +')
  print(f'           <input type="number" name="d{ch+1}2" placeholder="d{ch+1}2" value="{ch_conf[ch]["D"][2]}"> &bull; ADC2 +')
  print(f'           <input type="number" name="d{ch+1}3" placeholder="d{ch+1}3" value="{ch_conf[ch]["D"][3]}"> &bull; ADC3 +')
  print(f'           <input type="number" name="d{ch+1}4" placeholder="d{ch+1}4" value="{ch_conf[ch]["D"][4]}"> &bull; ADC4')
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
      print(' >C,D MISO matrix</option>')
      print('        </select>')
      print('      </td>')
      print('    </tr>')

    #-------  PID Gp -----------------------------
    print('    <tr>')
    print(f'      <td>G<sub>prop</sub> = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_gp_ch{ch+1}" id="pid{pidinst}_gp_ch{ch+1}" value="{ch_conf[ch]["PID"+pidinst+"_Gp"]}">')
    print('      </td>')
    print('    </tr>')
  
    #-------  PID Gi -----------------------------
    print('    <tr>')
    print(f'      <td>G<sub>integr</sub> = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_gi_ch{ch+1}" id="pid{pidinst}_gi_ch{ch+1}" value="{ch_conf[ch]["PID"+pidinst+"_Gi"]}">')
    print('      </td>')
    print('    </tr>')
  
    #-------  PID G1d -----------------------------
    print('    <tr>')
    print(f'      <td>G<sub>deriv,1</sub> = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_g1d_ch{ch+1}" id="pid{pidinst}_g1d_ch{ch+1}" value="{ch_conf[ch]["PID"+pidinst+"_G1d"]}">')
    print('      </td>')
    print('    </tr>')
  
    #-------  PID G2d -----------------------------
    print('    <tr>')
    print(f'      <td>G<sub>deriv,2</sub> = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_g2d_ch{ch+1}" id="pid{pidinst}_g2d_ch{ch+1}" value="{ch_conf[ch]["PID"+pidinst+"_G2d"]}">')
    print('      </td>')
    print('    </tr>')

    #-------  PID G_AIW -----------------------------
    print('    <tr>')
    print(f'      <td>G<sub>int.windup</sub> = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_g_aiw_ch{ch+1}" id="pid{pidinst}_g_aiw_ch{ch+1}" value="{ch_conf[ch]["PID"+pidinst+"_G_aiw"]}"> (it should be 1)')
    print('      </td>')
    print('    </tr>')
  
    #-------  PID input dead band -----------------------------
    print('    <tr>')
    print(f'      <td>in deadband = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_in_thr_ch{ch+1}" id="pid{pidinst}_in_thr_ch{ch+1}" value="{ch_conf[ch]["PID"+pidinst+"_in_thr"]}"> (fullscale = &plusmn;1.0)')
    print('      </td>')
    print('    </tr>')
  
    #-------  PID output saturation -----------------------------
    print('    <tr>')
    print(f'      <td>out saturation = </td>')
    print('      <td>')
    print(f'       <input type="number" name="pid{pidinst}_out_sat_ch{ch+1}" id="pid{pidinst}_out_sat_ch{ch+1}" value="{ch_conf[ch]["PID"+pidinst+"_out_sat"]}"> (fullscale = &plusmn;1.0)')
    print('      </td>')
    print('    </tr>')
  
    #-------  PID derivative on PV -----------------------------
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

    print('    <tr><td>&nbsp</td></tr>')
    
    if(pidinst=='1'):
      #-------  2 IIR instances  nested inside the two PIDs-----------------------------
      for IIRinst in ['1','2']:  
        print('    <tr>')
        print('      <td colspan="2">')
        print('        <div class="fraction">')
        print('          <div class="numerator">')
        print(f'            IIR#{IIRinst}: Y(n)=')
        print(f'            <input type="number" name="iir{IIRinst}_a0" placeholder="a0" value="{ch_conf[ch]["IIR"+IIRinst+"_A0"]}"> &bull; X(n) +')
        print(f'            <input type="number" name="iir{IIRinst}_a1" placeholder="a1" value="{ch_conf[ch]["IIR"+IIRinst+"_A1"]}"> &bull; X(n-1) +')
        print(f'            <input type="number" name="iir{IIRinst}_a2" placeholder="a2" value="{ch_conf[ch]["IIR"+IIRinst+"_A2"]}"> &bull; X(n-2) -')
        print(f'            <input type="number" name="iir{IIRinst}_b1" placeholder="b1" value="{ch_conf[ch]["IIR"+IIRinst+"_B1"]}"> &bull; Y(n-1) -')
        print(f'            <input type="number" name="iir{IIRinst}_b2" placeholder="b2" value="{ch_conf[ch]["IIR"+IIRinst+"_B2"]}"> &bull; Y(n-2)')
        print('          </div>')
        print('        </div>')
        print('      </td>')
        print('    </tr>')
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
  print(f'           <input type="number" name="e{ch+1}0" placeholder="e{ch+1}0" value="{ch_conf[ch]["E"][0]}"> +')
  print(f'           <input type="number" name="e{ch+1}1" placeholder="e{ch+1}1" value="{ch_conf[ch]["E"][1]}"> &bull; CH1 +')
  print(f'           <input type="number" name="e{ch+1}2" placeholder="e{ch+1}2" value="{ch_conf[ch]["E"][2]}"> &bull; CH2 +')
  print(f'           <input type="number" name="e{ch+1}3" placeholder="e{ch+1}3" value="{ch_conf[ch]["E"][3]}"> &bull; CH3 +')
  print(f'           <input type="number" name="e{ch+1}4" placeholder="e{ch+1}4" value="{ch_conf[ch]["E"][4]}"> &bull; CH4 ')
  print('         </div>')
  print('         <!-- Fraction line -->')
  print('         <div class="line"></div>')
  print('         <!-- Denominator -->')
  print('         <div class="denominator">')
  print(f'           <input type="number" name="f{ch+1}0" placeholder="f{ch+1}0" value="{ch_conf[ch]["F"][0]}"> +')
  print(f'           <input type="number" name="f{ch+1}1" placeholder="f{ch+1}1" value="{ch_conf[ch]["F"][1]}"> &bull; CH1 +')
  print(f'           <input type="number" name="f{ch+1}2" placeholder="f{ch+1}2" value="{ch_conf[ch]["F"][2]}"> &bull; CH2 +')
  print(f'           <input type="number" name="f{ch+1}3" placeholder="f{ch+1}3" value="{ch_conf[ch]["F"][3]}"> &bull; CH3 +')
  print(f'           <input type="number" name="f{ch+1}4" placeholder="f{ch+1}4" value="{ch_conf[ch]["F"][4]}"> &bull; CH4')
  print('         </div>')
  print('       </div>')
  print('       </td>')
  print('     </tr>')

  #-------  DAC out selector  -----------------------------
  print('    <tr>')
  print(f'      <td>DAC#{ch+1} driver:</td>')
  print('      <td>')
  print(f'        <select name="dac_select{ch+1}" id = "dac_select{ch+1}">')
  if ch_conf[ch]['outsel']=="CTRLLOOP" :
    print(f'          <option value = "0"         >waveform generator ch#{ch+1}</option>')
    print(f'          <option value = "1" selected>control loop ch#{ch+1}</option>')
  else:
    print(f'          <option value = "0" selected>waveform generator ch#{ch+1}</option>')
    print(f'          <option value = "1"         >control loop ch#{ch+1}</option>')  
  print('        </select>')
  print('      </td>')
  print('    </tr>')

  #-------  DAC offset  -----------------------------
  print('    <tr>')
  print(f'      <td>DAC#{ch+1} offset =</td>')
  print('      <td>')
  print(f'          <input type="number" name="dac_offs{ch+1}" id="dac_offs{ch+1}" value="{ch_conf[ch]["DACoffs"]}" min="-32768" max="32767" step=1> counts')
  print('      </td>')
  print('    </tr>')

  print('    <tr><td>&nbsp</td></tr>')

  #-------  Submit button  -----------------------------
  print('    <tr><td colspan="2">')
  print(f'     <button type="submit" name="updconf{ch+1}" id="updconf{ch+1}" value=1> update CH#{ch+1} configuration</button>')
  print('    </td></tr>')

  #-------  last spacer  -----------------------------
  if(ch==3):
    print('    <tr><td colspan="2" class="divider"><hr /></td></tr>')
  print('  </table>')

  print('</form>')

print('<br><br>')
print('</body>')
print('</html>')

s.close


