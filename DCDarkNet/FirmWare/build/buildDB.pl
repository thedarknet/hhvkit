#!/usr/bin/perl

$numCodes = 350;
#$numCodes = 10;

use Math::Random::ISAAC;

my $seed = 1;
open SEED, "/dev/urandom" or die "Couldn't open /dev/urandom to get a seed";
for ($ndx = 0; $ndx < 16; $ndx++) {
   read SEED, $foo, 1;
   $seed <<= 8;
   $seed += ord($foo);
}
close SEED;

printf "Seed: 0x%016X\n", $seed;
my $rng = Math::Random::ISAAC->new($seed);

for ($ndx = 0; $ndx < $numCodes; $ndx++) {
   # Make the GUID
   $guid = "";
   for ($i = 0; $i < 8; $i++) {
      $guid .= chr(65 + $rng->irand()%16);
   }

   printf "%s %04X %04X %04X %04X\n", $guid, 
      $rng->irand()%0x10000, $rng->irand()%0x10000, $rng->irand()%0x10000, $rng->irand()%0x10000;
}
