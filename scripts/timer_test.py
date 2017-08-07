#!/usr/bin/env python
import serial
import time
import sys


last_time = time.time()

with serial.Serial('/dev/ttyUSB0', 115200) as ser:
    while True:
        #print ser.in_waiting
        ln = ser.read()
        #sys.stdout.write(ln)
        if(ln == "\x01"):
            print "PING"
            print (time.time() - last_time)
            last_time = time.time()
