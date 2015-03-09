#!/usr/bin/perl


($guid) = @ARGV;

unless ($guid =~ m/^[A-Z0-9]{8}$/) {
   print STDERR "Usage: $0 [GUID]\n";
   exit(1);
}

open DB, "DB" or die "Couldn't open DB for reading: $!";
$key = "";
while (<DB>) {
   next unless m/$guid ([0-9A-F]{4}) ([0-9A-F]{4}) ([0-9A-F]{4}) ([0-9A-F]{4})/;
   $key = "$1$2$3$4";
   last;
}
close DB;

die "Couldn't find GUID in DB: $guid\nExiting.\n" if $key eq "";

my $eepromStr = "1403EC00$key";
foreach $char (split '', $guid) {
   $eepromStr .= sprintf "%02X", ord($char);
}
$eepromStr .= "00000000";

@chars = split '', $eepromStr;
$checksum = 0;
while (scalar(@chars)) {
   $a = shift(@chars);
   $b = shift(@chars);
   $checksum += hex("$a$b");
}
printf ":$eepromStr%02X\n", 256-($checksum%256);
print ":00000001FF\n";
