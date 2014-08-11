#!/usr/bin/perl

# This takes the codes from the tv-b-gone file and modifes them
# to work with the newer version of gcc that doesn't like the way
# the data structures were.

$DEBUG = 0;

while(<STDIN>) {
   if (m/const struct IrCode code_(\w\w\d+)Code/) {
      $code = $1;
      $DEBUG and print "DEBUG: Found struct $code\n";
      # Starting a struct.  Look for the codes block:
      @struct = ($_);
      while (<STDIN>) {
         if (m/^\s+{/) {
            $DEBUG and print "DEBUG: Found codes\n";
            # The start of the codes block.
            print "const uint8_t code_${code}Codes[] PROGMEM = {\n";
            while(<STDIN>) {
               if (m/^\s+}/) { # End of the Codes block
                  $DEBUG and print "DEBUG: End of Codes block.\n";
                  print "};\n";
                  last;
               }
               s/^\s+/\t/;
               print;
            }
            # Codes block is out, dump the struct and continue on.
            print @struct;
            print "\tcode_${code}Codes,\n";
            last;
         }
         push @struct, $_;
      }
   }
   else {
      print;
   }
}

