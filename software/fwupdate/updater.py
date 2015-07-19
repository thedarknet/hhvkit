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
import AVRProg
import sys

DEBUG = False

def modeReadFlash():
    print("Reading flash")
    flashFile = AVRProg.readFlash(DEBUG)
    if flashFile:
        print("Flash dumped to: " + flashFile)
    else:
        print("Error reading flash")

def modeReadEEPROM():
    print("Reading EEPROM")
    eepromFile = AVRProg.readEEPROM(DEBUG);
    print("EEPROM dumped to: " + eepromFile)
    if eepromFile:
        if DEBUG:
            AVRProg.dumpEEPROM(eepromFile)
    else:
        print("Error reading EEPROM")

def modeReadAll():
    modeReadFlash()
    AVRProg.dragonWait()
    modeReadEEPROM()

def modeFlashBootloader():
    print("Erasing Chip and Burning Fuses")
    
    lcd.clear();
    lcd.write("Burning Fuses")
    
    rval = AVRProg.runAvrdudeCommand('avrdude -v -c ' + AVRProg.PROGRAMMER + ' -p m328p -P ' + AVRProg.PROGRAMMERS[AVRProg.PROGRAMMER] + ' -e -Ulock:w:0x3F:m -Uefuse:w:0x05:m -Uhfuse:w:0xDE:m -Ulfuse:w:0xFF:m', DEBUG)
    
    if rval == 1:
        print("Error burning fuses")
        return rval

    AVRProg.dragonWait()

    lcd.clear();
    lcd.write("Flash Bootloader")
    
    rval = AVRProg.runAvrdudeCommand('avrdude -v -c ' + AVRProg.PROGRAMMER + ' -p m328p -P ' + AVRProg.PROGRAMMERS[AVRProg.PROGRAMMER] + ' -Uflash:w:./optiboot_atmega328.hex:i -Ulock:w:0x0F:m', DEBUG)
    if rval == 1:
        print("Error flashing bootloader")
        return rval

    return 0

MODES = {   'readFlash': modeReadFlash,
            'readEEPROM': modeReadEEPROM,
            'readAll': modeReadAll,
            'flashBootloader': modeFlashBootloader,
            'flashFW': None,
            'flashEEPROM': None,
            'updateFW': None}

# 
# Start Here!
# 
if len(sys.argv) < 3:
    print("Usage: " + sys.argv[0] + " <programmer> <mode>")
    print("\nSupported programmers(Use 'auto' if you're not sure):\n    " + '\n    '.join(AVRProg.PROGRAMMERS) + "")
    print("\nSupported Modes:\n    " + '\n    '.join(MODES) + "")
    sys.exit(1)

# 
# Setup LCD
# 
lcd = SFLCDController.Controller()
lcd.connect( "/dev/ttyO4", 9600)
lcd.clear()
lcd.write("DarkNet FWUpdate")

AVRProg.lcd = lcd

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

if not sys.argv[1] in AVRProg.PROGRAMMERS:
    if sys.argv[1] == 'auto':
        print('Searching all supported programmers for device')
        for key in AVRProg.PROGRAMMERS.keys():
            print("Checking " + key)

            returnCode = AVRProg.runAvrdudeCommand('avrdude -v -c ' + key + ' -p m328p -P ' + AVRProg.PROGRAMMERS[key], False)

            # We'll use the first one we find
            if returnCode == 0:
                print("Looks good!")
                AVRProg.PROGRAMMER = key
                break

        if AVRProg.PROGRAMMER == None:
            print("Could not find connected device")
            sys.exit(1)
        
        AVRProg.dragonWait()
    else:
        print("Supported programmers(Use 'auto' if you're not sure):\n    " + '\n    '.join(AVRProg.PROGRAMMERS) + "")
        sys.exit(1)
else:
    AVRProg.PROGRAMMER = sys.argv[1]

# 
# Do the thing!
# 
MODES[sys.argv[2]]()

lcd.disconnect()
