#!/usr/bin/python
# 
# Test program to update DCDarkNet badges using avrdude in FTDI(arduino) mode
# 
# 
import Adafruit_BBIO.GPIO as GPIO
import Adafruit_BBIO.UART as UART
import SFLCDController
import AVRProg
import os.path
import time
import sys

DEBUG = False

def readFlash():
    print('Reading flash')
    flashFile = AVRProg.readFlash(DEBUG)
    if flashFile:
        print('Flash dumped to: ' + flashFile)
    else:
        print('Error reading flash')

def flashFirmware(filename):
    lcd.clear();
    lcd.write('Flashing FW')

    rval = AVRProg.runAvrdudeCommand('avrdude -v -c ' + AVRProg.PROGRAMMER + ' -p m328p -P ' + AVRProg.PROGRAMMERS[AVRProg.PROGRAMMER] + ' -Uflash:w:' + filename + ':i', DEBUG)
    if rval == 1:
        print('Error flashing firmware')
        lcd.clear()
        lcd.write("ERROR: could not flash firmware")
        return rval
    else:
        print('Flashed firmware successfully!')
        lcd.clear()
        lcd.write("Success!!!")

    return 0

def waitForButton(pin):
    # 
    # Debounce!
    # 
    pushed = False
    while not pushed:    
        GPIO.wait_for_edge(pin, GPIO.FALLING)
        time.sleep(0.033)
        if (GPIO.input(pin) == 0):
            pushed = True

# 
# Start Here!
# 
if len(sys.argv) < 2:
    print('Usage: ' + sys.argv[0] + ' /path/to/firmware.bin')
    sys.exit(1)

GPIO.setup('P9_12', GPIO.IN)
UART.setup("UART4")

# 
# Setup LCD
# 
lcd = SFLCDController.Controller()
lcd.connect('/dev/ttyO4', 9600)
lcd.clear()

AVRProg.lcd = lcd
AVRProg.PROGRAMMER = 'arduino'

if os.path.exists('/dev/ttyUSB0') is False:
    lcd.clear()
    lcd.write('DCDarkNet ERR:  no /dev/ttyUSB0')
    
while os.path.exists('/dev/ttyUSB0') is False:
    time.sleep(5)

# TODO - add updater/error count
while True:
    lcd.clear()
    lcd.write('DCDarkNet - Push to update FW')
    waitForButton('P9_12')
    flashFirmware(sys.argv[1])
    time.sleep(5)
    # TODO - add animation here

lcd.disconnect()
