hhvkit
======

DefCon 23 Hardware Hacking Village Learn To Solder Kit, by:
- Smitty, mark@halibut.com (concept, crypto, and firmware (2014))
- Krux, krux@thcnet.net (hardware)
- Cmdc0de, comc0dez@gmail.com (firmware (2015))

These are the files needed to build the firmware for the DefCon DarkNet
ID Badge that was sold as the Hardware Hacking Village's Learn To Solder
kit at DefCon 23 in 2015.

If you checkout the repo into your Arduino folder, it should just build
on Arduino v1.6.4.  

It creates the following directories:

DCDarkNet/Firmware/
- Contains the code.

DCDarkNet/Firmware/build/
- The automated build/burn system used to program over 300 chips in 
  just a couple hours. (ZIF socket not included. Some assembly required.)

libraries/IRSerial2014/
- A modified version of SoftwareSerial from Arduino 1.0 (don't know if 
  it's changed at all in 1.5) to allow for independent inverted logic
  on the Rx and Tx lines (so the idle state isn't HIGH on the IR LED
  and burn through battery), and to perform 38kHz modulation for the IR.

libraries/UsbKeyboard/
- A version of vusb-for-arduino [1] modified so it doesn't instantiate
  the USB object in the .h file and therefore immeidately upon startup.
  Instead, you instantiate the object whenever you want it and can
  delete it when you're done.
  [1] https://code.google.com/p/vusb-for-arduino/


libraries/DarkNetDisplay
- Modified version of the adafruit gfx and ssd_1306 libraries to work with our 
  darknet badge display
	https://github.com/adafruit/Adafruit-GFX-Library
	https://github.com/adafruit/Adafruit_SSD1306

Building and Burning
======

All the following commands are run inside DCDarkNet/Firmware/build/
This section is really only relevant if you are burning this code into
a whole bunch of chips.  If you're just burning your own chip, or are
playing around with different code, you're better off just using the 
Arduino IDE to program the board directly with an FTDI cable.

Preparation: GUIDs and private keys
======
The code is programmed to FLASH, but there are a few things needed in
EEPROM before the badge is functional.  If you have a chip from us, it
was already programmed with your GUID and private key, which will persist
through a re-flash.  But if you have a new chip, you'll need to program
your GUID and private key into the chip.

We don't currently (2015-08-2) have a way to generate new GUIDs and 
keys for you, nor to take in GUIDs and keys from you to put into the DB.
So you're welcome to play with this code and use it for other things, 
but unless you got a chip from us at DefCon, you won't be able to 
use your code on the live DarkNet site (https://dcdark.net.)

If you're building a whole new name space and don't need to interact 
with the DefCon DarkNet Daemon, you can generate your own DB of random
GUIDs and private keys using the build/buildDB.pl script, and redirecting
the output to a file called DB.  This doesn't actually do anything with 
the chip, it just generates random numbers and formats them correctly.

build$ ./buildDB.pl > DB

Preparation: Program
=======
All the source code is compiled down into a .hex file by Arduino. Those
who do more microcontroller work than I do can build this by hand.  I'm 
lazy and let Arduino do all the drudgery for me.

Go to preferences and enable "verbose output of the build process" or
whatever it's called.  This will make Arduino NOT delete the temporary
build directories it creates in /tmp.  Then build the code with Ctrl-R.
That will burn into /tmp/build[some number].tmp/   Find the most recent
build directory (the one that was likely just created or updated), and
copy Firmware.cpp.hex to your build/ directory:

build$ cp /tmp/build[some number].tmp/Firmware.cpp.hex .

You will have to do this EVERY TIME you build.

Burning Chips
=======
The burn-bootloader.sh script assumes you have a USBtinyISP [2].  You'll
need to modify the avr-dude command if you have a different ISP.
[2] http://www.adafruit.com/product/46

Connect the USBtinyISP to the ICSP header on your board and run:

build$ ./burn-bootloader.sh

Congrats!  Your badge is now an Arduino!  It can be programmed through
the FTDI header as if it were an Uno.

But to make it a DarkNet ID Badge, it needs the GUID and private key in 
EEPROM, and the program in flash.

The burn-eeprom.sh script takes a GUID on the command line, pulls it 
and it's associated key from the "DB" file generated above, formats
them into a .hex file that avr-dude can burn to EEPROM, and burns it.
Again, you'll need the USBtinyISP plugged into the ICSP header:

build$ ./burn-eeprom.sh GUIDGUID

After those two are done, the program can be written to flash using
the FTDI serial console cable (it's much faster than using the ICSP.)
burn-flash.sh takes the local file Firmware.cpp.hex you copied 
above.

build$ ./burn-flash.sh

Automating the Build
======
I wasn't about to run these three commands manually for 350 chips, so 
I wrote run.pl which pulls all the GUIDs from DB and cycles through 
them.  If all is happy and working, you can just run.pl, hit enter to
program a chip, swap the chip in the programming rig, hit enter again, 
lather, rinse, repeat.  It takes care of all the iteration on GUID,
as well as allows you to <N>ext and <P>rev through the DB list in case
a burn goes awry and needs to be redone.

build$ ./run.pl

BOM and Assembly Instructions
======

The Bill of Materials is located here:

https://www.mouser.com/ProjectManager/ProjectDetail.aspx?AccessID=6bc2d848d8


Assembly Instructions are located here:

- https://dcdark.net/2015/hhv-badge/
- https://dcdark.net/2015/hhv-display/

