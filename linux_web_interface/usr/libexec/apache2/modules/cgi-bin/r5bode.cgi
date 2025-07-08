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
MAXSAMPLES=16383
MAXCNTS=32768.
# seconds to wait before checking again for end of acquisition
TRIG_IDLE_RETRY_DELAY_SEC=1

# acquisition state machine
ACQ_IDLE=0
ACQ_START=1
ACQ_WAIT_COMPLETION=2
ACQ_DOWNLOAD=3

# defaults
sweep_ch=4
in_ch=4
out_ch=1
fstart=100
fstop=4000
acq_state=ACQ_IDLE

# Ensure this works even if DISPLAY is not set (e.g., on servers)
import matplotlib
matplotlib.use('Agg')

# --------- open a connection to r5ctrlr SCPI server ----------
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 8888))
#s.connect(("192.168.0.18", 8888))


# ---------  get the query string of the GET form  ------------
# It is passed to cgi scripts as the environment
# variable QUERY_STRING
query_string = os.environ['QUERY_STRING']
#query_string = 'startacq=1'
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
  elif name=='acqstate':
    acq_state=int(arguments[name][0])
    #if arguments[name][0].strip()=='1':
    #  acquire_state=ACQ_START


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

# --- reload script for the state machine

if ((acq_state!=ACQ_IDLE) and (acq_state!=ACQ_DOWNLOAD)) :
  print('<script>')
  print('    window.addEventListener("load", () => {')
  print('      const url = new URL(window.location.href);')
  print('      const params = url.searchParams;')
  print('')
  
  if acq_state==ACQ_START:
    print(f'        params.set("acqstate", "{ACQ_WAIT_COMPLETION}");')
  elif acq_state==ACQ_WAIT_COMPLETION:
    print(f'        params.set("acqstate", "{ACQ_DOWNLOAD}");')
    
  print('        url.search = params.toString();')
  print('        window.location.replace(url.toString());')
  print('    });')
  print('  </script>')

# --- end of reload script

print('  <table>')
print('    <tr>')
print('      <td> <a href="?"> <img src="/MAX-IV_logo1_rgb-300x104.png" alt="MaxIV Laboratory"> </a> </td>')
print('      <td>')
print('      <H1>Max IV R5 controller</H1>')
print('      <H2>Bode Facility</H2>')
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

print(f'  <br>Acquisition time in this configuration is {acqtime:.3f} sec<br><br>')

print(f'  <button type="submit" name="acqstate" id="acqstate" value={ACQ_START}> Acquire </button>')

print('</form>')

# ------------------  acquire a new Bode  --------------------------

# ----- it's a state machine to allow page reload during acquisition
# ----- and data downloading, to give some feedback to the user

if acq_state==ACQ_IDLE :
  pass
elif acq_state==ACQ_START :
  # print('  <br>')
  # print(datetime.now())
  # print(' : configuring...')

  cmd_s='WAVEGEN:CH_ENABLE '+str(sweep_ch)+' OFF\n'
  s.sendall(cmd_s.encode('ascii')) 
  ans=(s.recv(1024)).decode('utf-8')
  tok=ans.split(" ",2)
  if tok[0].strip()=='ERR:':
    print('<br>Error programming the waveform generator (step 1)<br>')

  # by default sweep is done at 80% full scale, no offset
  cmd_s='WAVEGEN:CH_CONFIG '+str(sweep_ch)+' SWEEP 0.8 0 '+str(fstart)+' '+str(fstop)+' '+str(acqtime)+'\n'
  # print('<br>'+cmd_s+'<br>')
  s.sendall(cmd_s.encode('ascii')) 
  ans=(s.recv(1024)).decode('utf-8')
  tok=ans.split(" ",2)
  if tok[0].strip()=='ERR:':
    print('<br>Error programming the waveform generator (step 2)<br>')

  cmd_s='RECORD:TRIG:SETUP '+str(sweep_ch)+' SWEEP\n'
  s.sendall(cmd_s.encode('ascii')) 
  ans=(s.recv(1024)).decode('utf-8')
  tok=ans.split(" ",2)
  if tok[0].strip()=='ERR:':
    print('<br>Error programming the waveform generator (step 3)<br>')

  cmd_s='WAVEGEN ON\n'
  s.sendall(cmd_s.encode('ascii')) 
  ans=(s.recv(1024)).decode('utf-8')
  tok=ans.split(" ",2)
  if tok[0].strip()=='ERR:':
    print('Error programming the waveform generator (step 4)<br>')

  cmd_s='RECORD:TRIG ARM\n'
  s.sendall(cmd_s.encode('ascii')) 
  ans=(s.recv(1024)).decode('utf-8')
  tok=ans.split(" ",2)
  if tok[0].strip()=='ERR:':
    print('<br>Error programming the waveform generator (step 5)<br>')

  cmd_s='WAVEGEN:CH_ENABLE '+str(sweep_ch)+' SINGLE\n'
  s.sendall(cmd_s.encode('ascii')) 
  ans=(s.recv(1024)).decode('utf-8')
  tok=ans.split(" ",2)
  if tok[0].strip()=='ERR:':
    print('<br>Error programming the waveform generator (step 6)<br>')

  print('  <br>')
  print(datetime.now())
  print(f' : acquiring {acqtime:.3f} sec of data...')
  # here page will reload with new acq_state
    
elif acq_state==ACQ_WAIT_COMPLETION:
  ans=''
  while ans != 'IDLE' :
    cmd_s='RECORD:TRIG?\n'
    # print('<br>'+cmd_s+'<br>')
    s.sendall(cmd_s.encode('ascii')) 
    ans=(s.recv(1024)).decode('utf-8')
    tok=ans.split(" ",2)
    if tok[0].strip()=='ERR:':
      print('<br>'+ans+'<br>')
      break
    ans=tok[1].strip()
    time.sleep(TRIG_IDLE_RETRY_DELAY_SEC)
  
  print('  <br>')
  print(datetime.now())
  print(' : downloading samples')
  # here page will reload with new acq_state
    
elif acq_state==ACQ_DOWNLOAD:
  # -------------  read samples  ------------------
  cmd_s='RECORD:SAMPLES?\n'
  s.sendall(cmd_s.encode('ascii'))
  # now receive a big answer 
  samplebuf=""
  while True:
    s.setblocking(0)
    ## set timeout to twice the acquisition time
    #timeout_in_seconds=2*acqtime
    timeout_in_seconds=1
    ready = select.select([s], [], [], timeout_in_seconds)
    if ready[0]:
      ans=s.recv(1024)
      samplebuf += ans.decode('utf-8')
    else:
      break

  # print('  <br>')
  # print(datetime.now())
  # print(' : acquired ')

  buflines=samplebuf.splitlines()
  
  # while '\n' in samplebuf:
  #   line, samplebuf = samplebuf.split('\n', 1)
  #   print(f"Received line: {line}<br>")
  
  # first line is a header which we ignore
  if len(buflines) > 1:
    samples = [list(map(int, line.split())) for line in buflines[1:]]

  #print("2D Integer Array:<br>")
  #for row in samples:
  #  print(row)
  #  print('<br>')

  # if we have an odd number of samples, discard the first (which should be zero)
  if len(samples)%2 == 1 :
    samples=samples[1:]
  samplenum=len(samples)
  
  # print(f'{samplenum} samples<br>')
  
  # ------------- calculate something --------------
  Ts=1./fsampl
  timev=np.arange(samplenum)*Ts
  inv =np.array([sublist[ in_ch-1] for sublist in samples])/MAXCNTS
  outv=np.array([sublist[out_ch-1] for sublist in samples])/MAXCNTS
  df=(fsampl*1.0)/samplenum;
  freqv=np.arange(0,int(samplenum/2))*df
  # fin =np.fft.fft( inv*np.hanning(len(inv)))
  # fout=np.fft.fft(outv*np.hanning(len(outv)))
  fin =np.fft.fft( inv)
  fout=np.fft.fft(outv)
  h=np.divide(fout,fin)
  mag=20*np.log10(np.absolute(h))
  mag2=mag[:int(samplenum/2)]
  pha=np.angle(h,deg=True)
  pha2=pha[:int(samplenum/2)]
  #pha2=np.unwrap(pha2,period=360)
  
  # print(f'freqv size: {np.size(freqv)}<br>')
  # print(f'mag2 size: {np.size(mag2)}<br>')

  # -------------  plot  ------------------
  
  #------  Bode mag  ---------------
  plt.figure(figsize=(6, 4))
  plt.semilogx(freqv, mag2, linestyle='-', color='blue')
  plt.title('Bode magnitude')
  plt.xlabel('Hz')
  plt.ylabel('dB')
  plt.grid(True)
  plt.xlim([fstart,fstop])

  # Save plot to in-memory buffer
  imgbuf = io.BytesIO()
  plt.savefig(imgbuf, format='png')
  plt.close()
  imgbuf.seek(0)
  
  # Encode image to base64
  img_base64 = base64.b64encode(imgbuf.read()).decode('utf-8')
  
  # embed picture into html
  print(f'  <img src="data:image/png;base64,{img_base64}" alt="Bode mag">')
  
  #------  Bode phase  ---------------
  plt.figure(figsize=(6, 4))
  plt.semilogx(freqv, pha2, linestyle='-', color='blue')
  plt.title('Bode phase')
  plt.xlabel('Hz')
  plt.ylabel('deg')
  plt.grid(True)
  plt.xlim([fstart,fstop])

  # Save plot to in-memory buffer
  imgbuf = io.BytesIO()
  plt.savefig(imgbuf, format='png')
  plt.close()
  imgbuf.seek(0)
  
  # Encode image to base64
  img_base64 = base64.b64encode(imgbuf.read()).decode('utf-8')
  
  # embed picture into html
  print(f'  <img src="data:image/png;base64,{img_base64}" alt="Bode mag">')

  #------ sys input samples in time -------------
  plt.figure(figsize=(6, 4))
  plt.plot(timev, inv, linestyle='-', color='red')
  plt.title('system input in time')
  plt.xlabel('time')
  plt.ylabel('normalized output')
  plt.grid(True)
  
  # Save plot to in-memory buffer
  imgbuf = io.BytesIO()
  plt.savefig(imgbuf, format='png')
  plt.close()
  imgbuf.seek(0)
  
  # Encode image to base64
  img_base64 = base64.b64encode(imgbuf.read()).decode('utf-8')
  
  # embed picture into html
  print(f'  <img src="data:image/png;base64,{img_base64}" alt="Bode mag">')

  #------ sys output samples in time -------------
  plt.figure(figsize=(6, 4))
  plt.plot(timev, outv, linestyle='-', color='red')
  plt.title('system output in time')
  plt.xlabel('time')
  plt.ylabel('normalized output')
  plt.grid(True)
  
  # Save plot to in-memory buffer
  imgbuf = io.BytesIO()
  plt.savefig(imgbuf, format='png')
  plt.close()
  imgbuf.seek(0)
  
  # Encode image to base64
  img_base64 = base64.b64encode(imgbuf.read()).decode('utf-8')
  
  # embed picture into html
  print(f'  <img src="data:image/png;base64,{img_base64}" alt="Bode mag">')

  # ----------  embed raw data for download  ------------
  print('<br>')
  print('Raw data linked <a href="data:text/plain;charset=US-ASCII,')
  
  s='Nsamples '+str(samplenum)+'%0A'
  s.encode("ascii")
  print(s)
  s='Fsampling '+str(int(fsampl))+'%0A'
  s.encode("ascii")
  print(s)
  s='Fstart '+str(fstart)+'%0A'
  s.encode("ascii")
  print(s)
  s='Fstop '+str(fstop)+'%0A'
  s.encode("ascii")
  print(s)
  
  s=' Sys_input Sys_output%0A'
  s.encode("ascii")
  print(s)
  for row in samples:
    #s=str(row[in_ch-1])+' '+str(row[out_ch-1])+'%0A'
    s=f'{row[in_ch-1]:10} {row[out_ch-1]:10}%0A'
    s.encode("ascii")
    print(s)
  
  print('">here</a> if you want to download it.')
  
  
print('<br><br>')
print('</body>')
print('</html>')

s.close


