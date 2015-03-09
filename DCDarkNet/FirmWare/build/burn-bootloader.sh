#!/bin/bash
AVR_DUDE="/home/mark/Arduino/arduino-1.5.7/hardware/tools/avr/bin/avrdude"
AVR_DUDE_CONF="/home/mark/Arduino/arduino-1.5.7/hardware/tools/avr/etc/avrdude.conf"

# Burn fuses
$AVR_DUDE -C$AVR_DUDE_CONF -patmega328p -cusbtiny -e -Ulock:w:0x3F:m -Uefuse:w:0x05:m -Uhfuse:w:0xDE:m -Ulfuse:w:0xFF:m

# Burn boot loader into flash
$AVR_DUDE -C$AVR_DUDE_CONF -patmega328p -cusbtiny -Uflash:w:./optiboot_atmega328.hex:i -Ulock:w:0x0F:m


