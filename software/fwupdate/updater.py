#!/usr/bin/python
# 
# Test program to update DCDarkNet badges using avrdude
# Right now it just reads the flash and eeprom using avrispmkII
# but it will be able to program new badges and update old ones.
# Still need to add support for AVRDragon and plain FTDI
# 
import Adafruit_BBIO.GPIO as GPIO
import SFLCDController
import subprocess
import tempfile
import time
import sys
import os

# Max number of '#' characters in progress bar
AVRDUDE_PROGRESS_MAX = 50.0

# 
# Strings that precede a progress bar in avrdude stderr output
# along with the respective LCD string to display while this happens
# 
pbStrings = [
    ['reading flash memory','Reading Flash'],
    ['reading eeprom memory','Reading EEPROM']]

errorStrings = [
    ['No such device', 'ERR: Programmer'],
    ['did not find any USB device', 'ERR: Programmer'],
    ['Target not detected', 'ERR: No Target'],
    ]

def getStringWithArray(line, stringArray):
    for index in range(len(stringArray)):
        if stringArray[index][0] in line:
            return stringArray[index][1]

    return None

def getProgressBarString(line):
    return getStringWithArray(line, pbStrings)

def getErrorString(line):
    return getStringWithArray(line, errorStrings)

def runAvrdudeCommand(command, debugPrint = False):
    liveread = False
    line = ''           # Current line being read
    hashCount = 0       # Number of '#'s which represent the progress bar

    proc = subprocess.Popen(command.split(' '), stderr=subprocess.PIPE)
    while proc.poll() is None:
        
        if liveread:
            char = proc.stderr.read(1)
            line += char
            
            # Display in serial console
            if debugPrint:
                sys.stdout.write(char)
                sys.stdout.flush()

            if char == '\n' or char == '\r':
                # Only finish 'progress bar' mode after a newline
                # when the progress bar is actually displayed
                if 'Reading' in line:
                    liveread = False

                line = ''
                hashCount = 0

            # Progress bar is updating!
            if char == '#':
                hashCount += 1
                progress = hashCount / AVRDUDE_PROGRESS_MAX
                lcd.setCursorPos(16)
                lcd.write(str(progress * 100) + "%")

        else:
            line = proc.stderr.readline()
            
            errString = getErrorString(line)
            if errString:
                print(errString)
                lcd.clear()
                lcd.write(errString)

            pbString = getProgressBarString(line)
            if pbString:
                liveread = True
                line = ''
                lcd.clear()
                lcd.write(pbString)

            if debugPrint:
                if line != '':
                    print(line.rstrip())

def deleteFile(filename):
    try:
        os.remove(filename)
    except OSError:
        pass

def readFlash(debugPrint = False):
    filename = tempfile.gettempdir() + '/flash.bin'
    deleteFile(filename)

    runAvrdudeCommand('avrdude -v -c avrispmkII -p m328p -P usb -U flash:r:' + filename + ':r', debugPrint)

def readEEPROM(debugPrint = False):
    filename = tempfile.gettempdir() + '/eeprom.bin'
    deleteFile(filename)

    runAvrdudeCommand('avrdude -v -c avrispmkII -p m328p -P usb -U eeprom:r:' + filename + ':r', debugPrint)

    if not os.path.isfile(filename):
        filename = None

    return filename

# Test function to dump eeprom file
def dumpEEPROM(filename):
    f = open(filename, "rb")
    try:
        byte = f.read(1)
        while byte != '':
            sys.stdout.write(hex(ord(byte[0])) + ' ')
            byte = f.read(1)

    finally:
        f.close()

# 
# Setup LCD
# 
lcd = SFLCDController.Controller()
lcd.connect( "/dev/ttyO4", 9600)
lcd.clear()
lcd.write("DarkNet FWUpdate")

# 
# Do other stuff
# 
print("Reading flash")
readFlash(True)
print("Done reading flash")

print("Reading EEPROM")
eepromfile = readEEPROM(True);
print("Done reading EEPROM")
if eepromfile:
    dumpEEPROM(eepromfile)

lcd.disconnect()
