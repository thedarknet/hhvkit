#!/bin/bash

AVR_DUDE="/home/mark/Arduino/arduino-1.5.7/hardware/tools/avr/bin/avrdude"
AVR_DUDE_CONF="/home/mark/Arduino/arduino-1.5.7/hardware/tools/avr/etc/avrdude.conf"

GUID=$1

echo "Making EEPROM code for $GUID"
./make-eeprom.pl $1 > eeprom.hex

echo "EEPROM code for $GUID is:"
cat eeprom.hex

$AVR_DUDE -C$AVR_DUDE_CONF -patmega328p -cusbtiny -Ueeprom:w:eeprom.hex:i

