#!/usr/bin/python
# 
# Test program to update DCDarkNet badges using avrdude
# Right now it just reads the flash and eeprom using 
# avrispmkII, avr dragon isp, and FTDI(arduino)
# 
# NOTE: EEPROM data seems to be invalid while using FTDI/arduino bootloader
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
# Supported programmers
# 
PROGRAMMERS = { 'avrispmkII': 'usb',
                'dragon_isp': 'usb',
                'arduino': '/dev/ttyUSB0'}

PROGRAMMER = None

DEBUG = False

# 
# Strings that precede a progress bar in avrdude stderr output
# along with the respective LCD string to display while this happens
# 
pbStrings = [   ['reading flash memory','Reading Flash'],
                ['reading eeprom memory','Reading EEPROM'] ]

errorStrings = [    ['No such device', 'ERR: Programmer'],
                    ['did not find any USB device', 'ERR: Programmer'],
                    ['Target not detected', 'ERR: No Target'] ]

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

    return proc.returncode

def deleteFile(filename):
    try:
        os.remove(filename)
    except OSError:
        pass

def readFlash(debugPrint = False):
    filename = tempfile.gettempdir() + '/flash.bin'
    deleteFile(filename)

    runAvrdudeCommand('avrdude -v -c ' + PROGRAMMER + ' -p m328p -P ' + PROGRAMMERS[PROGRAMMER] + ' -U flash:r:' + filename + ':r', debugPrint)

    if not os.path.isfile(filename):
        filename = None

    return filename

def readEEPROM(debugPrint = False):
    filename = tempfile.gettempdir() + '/eeprom.bin'
    deleteFile(filename)

    runAvrdudeCommand('avrdude -v -c ' + PROGRAMMER + ' -p m328p -P ' + PROGRAMMERS[PROGRAMMER] + ' -U eeprom:r:' + filename + ':r', debugPrint)

    if not os.path.isfile(filename):
        filename = None

    return filename

# Test function to dump eeprom file
def dumpEEPROM(filename):
    f = open(filename, "rb")
    byteCount = 0
    try:
        byte = f.read(1)
        while byte != '':
            byteCount += 1
            sys.stdout.write(hex(ord(byte[0])) + ' ')
            if byteCount % 8 == 0:
                sys.stdout.write('\n')
            byte = f.read(1)


        sys.stdout.write('\n')

    finally:
        f.close()

def dragonWait():
    # The dragon doesn't like it when you try to re-connect too quickly
    if PROGRAMMER == 'dragon_isp':
        time.sleep(2)

def modeReadFlash():
    print("Reading flash")
    flashFile = readFlash(DEBUG)
    if flashFile:
        print("Flash dumped to: " + flashFile)
    else:
        print("Error reading flash")

def modeReadEEPROM():
    print("Reading EEPROM")
    eepromFile = readEEPROM(DEBUG);
    print("EEPROM dumped to: " + eepromFile)
    if eepromFile:
        if DEBUG:
            dumpEEPROM(eepromFile)
    else:
        print("Error reading EEPROM")

def modeReadAll():
    modeReadFlash()

    dragonWait()

    modeReadEEPROM()

MODES = {   'readFlash': modeReadFlash,
            'readEEPROM': modeReadEEPROM,
            'readAll': modeReadAll,
            'flashBootloader': None,
            'flashFW': None,
            'flashEEPROM': None,
            'updateFW': None}

# 
# Start Here!
# 
if len(sys.argv) < 3:
    print("Usage: " + sys.argv[0] + " <programmer> <mode>")
    print("\nSupported programmers(Use 'auto' if you're not sure):\n    " + '\n    '.join(PROGRAMMERS) + "")
    print("\nSupported Modes:\n    " + '\n    '.join(MODES) + "")
    sys.exit(1)

# 
# Setup LCD
# 
lcd = SFLCDController.Controller()
lcd.connect( "/dev/ttyO4", 9600)
lcd.clear()
lcd.write("DarkNet FWUpdate")

# 
# Validate user input
# 
if not sys.argv[2] in MODES:
    print("Unsupported mode '" + sys.argv[2] + "'")
    print("Supported Modes:\n    " + '\n    '.join(MODES) + "")
    sys.exit(1)
elif MODES[sys.argv[2]] == None:
    print("Mode '" + sys.argv[2] + "' not yet implemented")
    sys.exit(1)

if not sys.argv[1] in PROGRAMMERS:
    if sys.argv[1] == 'auto':
        print('Searching all supported programmers for device')
        for key in PROGRAMMERS.keys():
            print("Checking " + key)

            returnCode = runAvrdudeCommand('avrdude -v -c ' + key + ' -p m328p -P ' + PROGRAMMERS[key], False)

            # We'll use the first one we find
            if returnCode == 0:
                print("Looks good!")
                PROGRAMMER = key
                break

        if PROGRAMMER == None:
            print("Could not find connected device")
            sys.exit(1)
        
        dragonWait()
    else:
        print("Supported programmers(Use 'auto' if you're not sure):\n    " + '\n    '.join(PROGRAMMERS) + "")
        sys.exit(1)
else:
    PROGRAMMER = sys.argv[1]

# 
# Do the thing!
# 
MODES[sys.argv[2]]()

lcd.disconnect()
