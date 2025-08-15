#!/usr/bin/python

import socket
#import select
import os
import urllib.parse
#import time
#from datetime import datetime
#import numpy as np
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
print('      <td> <a href="/index.html"> <img src="/MAX-IV_logo1_rgb-300x104.png" alt="MaxIV Laboratory"> </a> </td>')
print('      <td>')
print('      <H1>Max IV R5 controller</H1>')
print('      <H2>Home page</H2>')
print('      </td>')
print('    </tr>')
print('  </table>')
print('</head>')
print('<body>')


# --------- open a connection to r5ctrlr SCPI server ----------
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#s.connect(("127.0.0.1", 8888))
s.connect(("192.168.0.18", 8888))


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

for name in arguments.keys():

  # -------- global reset
  if name=='globreset':
    if int(arguments[name][0])==1:
      cmd_s='*RST\n'
      s.sendall(cmd_s.encode('ascii')) 
      ans=(s.recv(1024)).decode('utf-8')


# --------------------  now display html page  -----------------------

print('')
print('Available pages:<br>')
print('<br>')
print('<a href="/cgi-bin/r5bode.cgi">Bode facility</a>')
print('<br>')
print('<a href="/cgi-bin/r5wgen.cgi">Waveform Generator facility</a>')
print('<br>')
print('<a href="/cgi-bin/r5ctrlloop.cgi">Real time controller</a>')
print('<br><br>')
print('<form>')
print('  <button type="submit" name="globreset" id="globreset" value=1>Global Controller Reset</button>')
print('</form>')

print('<br><br>')
print('</body>')
print('</html>')

s.close
