#!/usr/bin/perl

$AVR_DUDE = "/home/mark/Arduino/arduino-1.5.7/hardware/tools/avr/bin/avrdude";
$AVR_DUDE_CONF = "/home/mark/Arduino/arduino-1.5.7/hardware/tools/avr/etc/avrdude.conf";

open DB, "DB" or die "Couldn't open DB file: $!";
while(<DB>) {
   next if m/Seed:/;
   chomp;
   ($guid, $k0, $k1, $k2, $k3) = split " ", $_;

   #print "Loading GUID: $guid\n";
   push @guids, $guid;
}
close DB;

$numGuids = scalar(@guids);
print "Ready to program!\n";
for ($ndx=0; $ndx<$numGuids; $ndx++) {
   print "\n\n<Q>uit\n";
   print "<\\d+> Jump to #\$1\n";
   print "<N>ext (GUID: ", $guids[$ndx+1], ")\n" unless $ndx == $numGuids-1;
   print "<P>rev (GUID: ", $guids[$ndx-1], ")\n" unless $ndx == 0;
   print "Current GUID: #$ndx $guids[$ndx]\n";
   print "<Enter> to program with current\n";

   $foo = <STDIN>;
   chomp $foo;
   exit if $foo =~ m/q/i;
   if ($foo =~ m/(\d+)/) { $ndx = $1-1; next; }
   if ($ndx < $numGuids && $foo =~ m/n/i) { next; }
   if ($ndx > 0 && $foo =~ m/p/i) { $ndx-=2; next; }
   unless ($foo =~ m/^$/) { $ndx--; next; }

   

   # Burn fuses and boot loader
   print "Burning fuses and boot loader...\n";
   #system("$AVR_DUDE -C$AVR_DUDE_CONF -patmega328p -cusbtiny -e -Ulock:w:0x3F:m -Uefuse:w:0x05:m -Uhfuse:w:0xDE:m -Ulfuse:w:0xFF:m");
   #system("$AVR_DUDE -C$AVR_DUDE_CONF -patmega328p -cusbtiny -Uflash:w:./optiboot_atmega328.hex:i -Ulock:w:0x0F:m");
   # system("./burn-bootloader.sh");

   # The board is now an Arduino.

   # Burn GUID and KEY into EEPROM
   print "Burning EEPROM for $guids[$ndx]...\n";
   #system("./make-eeprom.pl $guids[$ndx] > eeprom.hex");
   #system("$AVR_DUDE -C$AVR_DUDE_CONF -patmega328p -cusbtiny -Ueeprom:w:eeprom.hex:i");
   # system("./burn-eeprom.sh $guids[$ndx]");

   # Upload the code through the FTDI cable
   print "Burning FLASH...\n";
   #system("$AVR_DUDE -C$AVR_DUDE_CONF -patmega328p -carduino -P/dev/ttyUSB0 -b115200 -D -Uflash:w:./IR_Quest_2014.cpp.hex:i");
   # system("./burn-flash.sh");

}
