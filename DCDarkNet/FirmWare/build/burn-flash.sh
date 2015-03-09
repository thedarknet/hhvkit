#!/bin/bash
AVR_DUDE="/home/mark/Arduino/arduino-1.5.7/hardware/tools/avr/bin/avrdude"
AVR_DUDE_CONF="/home/mark/Arduino/arduino-1.5.7/hardware/tools/avr/etc/avrdude.conf"

# Upload the code through the FTDI cable
$AVR_DUDE -C$AVR_DUDE_CONF -patmega328p -carduino -P/dev/ttyUSB0 -b115200 -D -Uflash:w:./IR_Quest_2014.cpp.hex:i

