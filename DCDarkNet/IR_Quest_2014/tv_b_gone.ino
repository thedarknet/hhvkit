/*
This is a direct rip-off of the TV-B-Gone v1.2 code downloaded
from Adafruit's site at:
http://learn.adafruit.com/system/assets/assets/000/010/188/original/firmwarev12.zip

I've pulled main.h and main.c into the same file (I know, I know, 
bad coder, I suck...  At least they're not in the main 
IR_Quest_2014 file...) and made modification as I go to fit in
with the rest of my code.  I've also made some translations into
Arduino instead of native AVR programming (eg: digitalWrite()
instead of fiddling with bit masks) just for readability.

But the real heavy lifting was done by the copyright holder below,
not my me.  Oh, "me" is:
-Mark Smith, 2014-07-30 in prep for The DefCon DarkNet Project
*/

/*
TV-B-Gone Firmware version 1.2
for use with ATtiny85v and v1.2 hardware
(c) Mitch Altman + Limor Fried 2009
Last edits, August 16 2009

With some code from:
Kevin Timmerman & Damien Good 7-Dec-07

Horribly mangled by Mark Smith 2014-07-30 to fit
into the DarkNet Badge, HHV Learn To Solder Kit, whatever it's called.

Distributed under Creative Commons 2.5 -- Attib & Share Alike

This is the 'universal' code designed for v1.2 - it will select EU or NA
depending on a pulldown resistor on pin B1 !
*/

//#include <avr/wdt.h>

// This is from main.h
// Two regions!
#define US 0
#define EU 1

// Shortcut to insert single, non-optimized-out nop
#define NOP __asm__ __volatile__ ("nop")

// Tweak this if neccessary to change timing
#define DELAY_CNT 29

void xmitCodeElement(uint16_t ontime, uint16_t offtime, uint8_t PWM_code );
void flashslowLEDx( uint8_t num_blinks );
void quickflashLEDx( uint8_t x );
void tvbgone_sleep( void );
void delay_ten_us(uint16_t us);
void quickflashLED( void );

// This is from main.c, except the copyright is moved up there, and I 
// didn't #include most of what main.c included since we already had most
// of it.  One exception is wdt.h above.

/*
This project transmits a bunch of TV POWER codes, one right after the other, 
with a pause in between each.  (To have a visible indication that it is 
transmitting, it also pulses a visible LED once each time a POWER code is 
transmitted.)  That is all TV-B-Gone does.  The tricky part of TV-B-Gone 
was collecting all of the POWER codes, and getting rid of the duplicates and 
near-duplicates (because if there is a duplicate, then one POWER code will 
turn a TV off, and the duplicate will turn it on again (which we certainly 
do not want).  I have compiled the most popular codes with the 
duplicates eliminated, both for North America (which is the same as Asia, as 
far as POWER codes are concerned -- even though much of Asia USES PAL video) 
and for Europe (which works for Australia, New Zealand, the Middle East, and 
other parts of the world that use PAL video).

Before creating a TV-B-Gone Kit, I originally started this project by hacking 
the MiniPOV kit.  This presents a limitation, based on the size of
the Atmel ATtiny2313 internal flash memory, which is 2KB.  With 2KB we can only 
fit about 7 POWER codes into the firmware's database of POWER codes.  However,
the more codes the better! Which is why we chose the ATtiny85 for the 
TV-B-Gone Kit.

This version of the firmware has the most popular 100+ POWER codes for 
North America and 100+ POWER codes for Europe. You can select which region 
to use by soldering a 10K pulldown resistor.
*/


/*
This project is a good example of how to use the AVR chip timers.
*/

// Hardware description left out since it no longer applies.

//extern const PGM_P *NApowerCodes[] PROGMEM;
//extern const PGM_P *EUpowerCodes[] PROGMEM;
//extern const uint8_t num_NAcodes, num_EUcodes;

/* This function is the 'workhorse' of transmitting IR codes.
   Given the on and off times, it turns on the PWM output on and off
   to generate one 'pair' from a long code. Each code has ~50 pairs! */
void xmitCodeElement(uint16_t ontime, uint16_t offtime, uint8_t PWM_code )
{
  // start Timer0 outputting the carrier frequency to IR emitters on and OC0A 
  // (PB0, pin 5)
  //TCNT0 = 0; // reset the timers so they are aligned
  //TIFR = 0;  // clean out the timer flags
  // Timers are setup in the tv_b_gone() (formerly main()) method below
  
  if(PWM_code) {
    // 99% of codes are PWM codes, they are pulses of a carrier frequecy
    // Usually the carrier is around 38KHz, and we generate that with PWM
    // timer 0
    //TCCR0A =_BV(COM0A0) | _BV(WGM01);          // set up timer 0
    //TCCR0B = _BV(CS00);
    TCCR1A = (TCCR1A & 0x3F) | 0x40;
  } else {
    // However some codes dont use PWM in which case we just turn the IR
    // LED on for the period of time.
    digitalWrite(IR_TX, HIGH);
    //PORTB &= ~_BV(IRLED);
  }

  // Now we wait, allowing the PWM hardware to pulse out the carrier 
  // frequency for the specified 'on' time
  delay_ten_us(ontime);
  //delayMicroseconds(ontime*10);

  // Now we have to turn it off so disable the PWM output
  //TCCR0A = 0;
  //TCCR0B = 0;
  TCCR1A &= 0x3F;
  
  // And make sure that the IR LED is off too (since the PWM may have 
  // been stopped while the LED is on!)
  digitalWrite(IR_TX, LOW);
  //PORTB |= _BV(IRLED);           // turn off IR LED

  // Now we wait for the specified 'off' time
  delay_ten_us(offtime);
  //delayMicroseconds(offtime*10);
}

/* This is kind of a strange but very useful helper function
   Because we are using compression, we index to the timer table
   not with a full 8-bit byte (which is wasteful) but 2 or 3 bits.
   Once code_ptr is set up to point to the right part of memory,
   this function will let us read 'count' bits at a time which
   it does by reading a byte into 'bits_r' and then buffering it. */

uint8_t bitsleft_r = 0;
uint8_t bits_r=0;
PGM_P code_ptr;

// we cant read more than 8 bits at a time so dont try!
uint8_t read_bits(uint8_t count)
{
  uint8_t i;
  uint8_t tmp=0;

  // we need to read back count bytes
  for (i=0; i<count; i++) {
    // check if the 8-bit buffer we have has run out
    if (bitsleft_r == 0) {
      // in which case we read a new byte in
      bits_r = pgm_read_byte(code_ptr++);
      // and reset the buffer size (8 bites in a byte)
      bitsleft_r = 8;
    }
    // remove one bit
    bitsleft_r--;
    // and shift it off of the end of 'bits_r'
    tmp |= (((bits_r >> (bitsleft_r)) & 1) << (count-1-i));
  }
  // return the selected bits in the LSB part of tmp
  return tmp;
}

/*
The C compiler creates code that will transfer all constants into RAM when 
the microcontroller resets.  Since this firmware has a table (powerCodes) 
that is too large to transfer into RAM, the C compiler needs to be told to 
keep it in program memory space.  This is accomplished by the macro PROGMEM 
(this is used in the definition for powerCodes).  Since the C compiler assumes 
that constants are in RAM, rather than in program memory, when accessing
powerCodes, we need to use the pgm_read_word() and pgm_read_byte macros, and 
we need to use powerCodes as an address.  This is done with PGM_P, defined 
below.  
For example, when we start a new powerCode, we first point to it with the 
following statement:
    PGM_P thecode_p = pgm_read_word(powerCodes+i);
The next read from the powerCode is a byte that indicates the carrier 
frequency, read as follows:
      const uint8_t freq = pgm_read_byte(code_ptr++);
After that is a byte that tells us how many 'onTime/offTime' pairs we have:
      const uint8_t numpairs = pgm_read_byte(code_ptr++);
The next byte tells us the compression method. Since we are going to use a
timing table to keep track of how to pulse the LED, and the tables are 
pretty short (usually only 4-8 entries), we can index into the table with only
2 to 4 bits. Once we know the bit-packing-size we can decode the pairs
      const uint8_t bitcompression = pgm_read_byte(code_ptr++);
Subsequent reads from the powerCode are n bits (same as the packing size) 
that index into another table in ROM that actually stores the on/off times
      const PGM_P time_ptr = (PGM_P)pgm_read_word(code_ptr);
*/

// Renaming to tv_b_gone() so we can call it from loop()
//int main(void) {
int tv_b_gone(void) {
  Serial.println(F("TV-B-GONE!"));
  uint16_t ontime, offtime;
  uint8_t i,j;
  uint8_t region = US;     // by default our code is US

  uint8_t oldSREG = SREG;
  cli();  // turn off interrupts for a clean txmit

  // Save current PWM state to be restored later
  uint8_t oldTCCR1A = TCCR1A;
  uint8_t oldTCCR1B = TCCR1B;
  uint16_t oldTCNT1 = TCNT1;
  uint16_t oldOCR1A = OCR1A;
  uint16_t oldICR1 = ICR1;
  
  // These are what I'm trying to recreate here. These are from ATtiny85
  //TCCR0A =_BV(COM0A0) | _BV(WGM01);          // set up timer 0
  // WGM0[210] = 2  CTC mode, TOP = OCRA
  // COM0A[10] = 1  Bit toggle mode
  //TCCR0B = _BV(CS00);
  // CS0[210] = 1   No prescale
  
  // I'm recreating it with ATmega328, and on Timer1 instead of 0
  TCCR1A = (TCCR1A & 0x3C) | 0x00;  // Start with no output.
  TCCR1B = (TCCR1B & 0xE0) | 0x09;
  
  //TCCR1 = 0;               // Turn off PWM/freq gen, should be off already
  //TCCR0A = 0;
  //TCCR0B = 0;

  // Not doing Watchdog timers.
  //i = MCUSR;                     // Save reset reason
  //MCUSR = 0;                     // clear watchdog flag
  //WDTCR = _BV(WDCE) | _BV(WDE);  // enable WDT disable

  //WDTCR = 0;                     // disable WDT while we setup

  // This is all done already in setup()
  //DDRB = _BV(LED) | _BV(IRLED);   // set the visible and IR LED pins to outputs
  //PORTB = _BV(LED) |              //  visible LED is off when pin is high
  //        _BV(IRLED) |            // IR LED is off when pin is high
  //        _BV(REGIONSWITCH);      // Turn on pullup on region switch pin

  //DEBUGP(putstring_nl("Hello!"));

  // check the reset flags
  //if (i & _BV(BORF)) {    // Brownout
  //  // Flash out an error and go to sleep
  //  flashslowLEDx(2);
  //  tvbgone_sleep();
  //}

  delay_ten_us(5000);            // Let everything settle for a bit

  // determine region
  //if (PINB & _BV(REGIONSWITCH)) {
  if (digitalRead(TV_B_GONE_REGION)) {
    region = US; // US
    Serial.println(F("US codes."));
    //DEBUGP(putstring_nl("US"));
  } else {
    region = EU;
    Serial.println(F("EU codes."));
    //DEBUGP(putstring_nl("EU"));
  }

  // Tell the user what region we're in  - 3 is US 4 is EU
  quickflashLEDx(3+region);

  // Starting execution loop
  delay_ten_us(25000);

  // turn on watchdog timer immediately, this protects against
  // a 'stuck' system by resetting it
  //wdt_enable(WDTO_8S); // 1 second long timeout

  // Indicate how big our database is
  //DEBUGP(putstring("\n\rNA Codesize: "); putnum_ud(num_NAcodes););
  //DEBUGP(putstring("\n\rEU Codesize: "); putnum_ud(num_EUcodes););

  // We may have different number of codes in either database
  if (region == US) {
    j = num_NAcodes;
  } else {
    j = num_EUcodes;
  }
  Serial.print(F("NumCodes: "));
  Serial.println(j);
  
  // for every POWER code in our collection
  for(i=0 ; i < j; i++) {

    // print out the code # we are about to transmit
    //DEBUGP(putstring("\n\r\n\rCode #: "); putnum_ud(i));
    Serial.print(F("Code #: "));
    Serial.println(i);

    //To keep Watchdog from resetting in middle of code.
    //wdt_reset();

    // point to next POWER code, from the right database
    if (region == US) {
      code_ptr = (PGM_P)pgm_read_word(NApowerCodes+i);
    } else {
      code_ptr = (PGM_P)pgm_read_word(EUpowerCodes+i);
    }

    // print out the address in ROM memory we're reading
    //DEBUGP(putstring("\n\rAddr: "); putnum_uh(code_ptr));

    // Read the carrier frequency from the first byte of code structure
    const uint8_t freq = pgm_read_byte(code_ptr++);
    Serial.print(F("Freq: "));
    Serial.println(freq);
    // set OCR for Timer1 to output this POWER code's carrier frequency
    OCR1A = freq;

    // TODO: verify we won't overflow 16 bit timer.  I doubt we will, 
    // they're all going to be close to 38kHz which is 421 or 210
    // depending on how they have the timer setup.

    // Print out the frequency of the carrier and the PWM settings
    //DEBUGP(putstring("\n\rOCR1: "); putnum_ud(freq););
    //DEBUGP(uint16_t x = (freq+1) * 2; putstring("\n\rFreq: "); putnum_ud(F_CPU/x););

    // Get the number of pairs, the second byte from the code struct
    const uint8_t numpairs = pgm_read_byte(code_ptr++);
    //DEBUGP(putstring("\n\rOn/off pairs: "); putnum_ud(numpairs));

    // Get the number of bits we use to index into the timer table
    // This is the third byte of the structure
    const uint8_t bitcompression = pgm_read_byte(code_ptr++);
    //DEBUGP(putstring("\n\rCompression: "); putnum_ud(bitcompression));

    // Get pointer (address in memory) to pulse-times table
    // The address is 16-bits (2 byte, 1 word)
    PGM_P time_ptr = (PGM_P)pgm_read_word(code_ptr);
    code_ptr+=2;

    // Transmit all codeElements for this POWER code 
    // (a codeElement is an onTime and an offTime)
    // transmitting onTime means pulsing the IR emitters at the carrier 
    // frequency for the length of time specified in onTime
    // transmitting offTime means no output from the IR emitters for the 
    // length of time specified in offTime

    /*    
    // print out all of the pulse pairs
    for (uint8_t k=0; k<numpairs; k++) {
      uint8_t ti;
      ti = (read_bits(bitcompression)) * 4;
      // read the onTime and offTime from the program memory
      ontime = pgm_read_word(time_ptr+ti);
      offtime = pgm_read_word(time_ptr+ti+2);
      DEBUGP(putstring("\n\rti = "); putnum_ud(ti>>2); putstring("\tPair = "); putnum_ud(ontime));
      DEBUGP(putstring("\t"); putnum_ud(offtime));
      } 
    */

    Serial.print(F("NumPairs: "));
    Serial.println(numpairs);
    // For EACH pair in this code....
    for (uint8_t k=0; k<numpairs; k++) {
      //Serial.write('.');
      uint8_t ti;

      // Read the next 'n' bits as indicated by the compression variable
      // The multiply by 4 because there are 2 timing numbers per pair
      // and each timing number is one word long, so 4 bytes total!
      ti = (read_bits(bitcompression)) * 4;

      // read the onTime and offTime from the program memory
      ontime = pgm_read_word(time_ptr+ti);  // read word 1 - ontime
      offtime = pgm_read_word(time_ptr+ti+2);  // read word 2 - offtime

      // transmit this codeElement (ontime and offtime)
      xmitCodeElement(ontime, offtime, (freq!=0));
    }

    //Flush remaining bits, so that next code starts
    //with a fresh set of 8 bits.
    bitsleft_r=0;

    // delay 250 milliseconds before transmitting next POWER code
    delay_ten_us(25000);

    // visible indication that a code has been output.
    //Serial.println(F("Next!"));
    quickflashLED();
  }

  // We are done, no need for a watchdog timer anymore
  //wdt_disable();

  // flash the visible LED on PB0  4 times to indicate that we're done
  delay_ten_us(65500); // wait maxtime 
  delay_ten_us(65500); // wait maxtime 
  quickflashLEDx(4);

  // Restore timer configs
  TCCR1A = oldTCCR1A;
  TCCR1B = oldTCCR1B;
  TCNT1 = oldTCNT1;
  OCR1A = oldOCR1A;
  ICR1 = oldICR1;
  SREG = oldSREG; // turn interrupts back on

  Serial.println(F("TV-R-Gone!"));
  //tvbgone_sleep();
}


/****************************** SLEEP FUNCTIONS ********/

/* We aren't using this, we just go back to what we were doing
   when we're done with TV-B-Gone.
void tvbgone_sleep( void )
{
  // Shut down everything and put the CPU to sleep
  TCCR0A = 0;           // turn off frequency generator (should be off already)
  TCCR0B = 0;           // turn off frequency generator (should be off already)
  PORTB |= _BV(LED) |       // turn off visible LED
           _BV(IRLED);     // turn off IR LED

  wdt_disable();           // turn off the watchdog (since we want to sleep
  delay_ten_us(1000);      // wait 10 millisec

  MCUCR = _BV(SM1) |  _BV(SE);    // power down mode,  SE enables Sleep Modes
  sleep_cpu();                    // put CPU into Power Down Sleep Mode
}
*/
/****************************** LED AND DELAY FUNCTIONS ********/


// This function delays the specified number of 10 microseconds
// it is 'hardcoded' and is calibrated by adjusting DELAY_CNT 
// in main.h Unless you are changing the crystal from 8mhz, dont
// mess with this.
void delay_ten_us(uint16_t us) {
  uint8_t timer;
  while (us != 0) {
    // for 8MHz we want to delay 80 cycles per 10 microseconds
    // this code is tweaked to give about that amount.
    // Damn. Have to re-tweak for 16MHz.
    for (timer=0; timer <= DELAY_CNT; timer++) {
      NOP;
      NOP;
    }
    NOP;
    us--;
  }
  //delayMicroseconds(us*10);
}


// This function quickly pulses the visible LED (connected to PB0, pin 5)
// This will indicate to the user that a code is being transmitted
void quickflashLED( void ) {
  //PORTB &= ~_BV(LED);   // turn on visible LED at PB0 by pulling pin to ground
  digitalWrite(LED_PIN, HIGH);
  delay_ten_us(3000);   // 30 millisec delay
  digitalWrite(LED_PIN, LOW);
  //PORTB |= _BV(LED);    // turn off visible LED at PB0 by pulling pin to +3V
}

// This function just flashes the visible LED a couple times, used to
// tell the user what region is selected
void quickflashLEDx( uint8_t x ) {
  quickflashLED();
  while(--x) {
        //wdt_reset();
        delay_ten_us(15000);     // 150 millisec delay between flahes
        quickflashLED();
  }
  //wdt_reset();                // kick the dog
}

// This is like the above but way slower, used for when the tvbgone
// crashes and wants to warn the user
void flashslowLEDx( uint8_t num_blinks )
{
  uint8_t i;
  for(i=0;i<num_blinks;i++)
    {
      // turn on visible LED at PB0 by pulling pin to ground
      //PORTB &= ~_BV(LED);
      digitalWrite(LED_PIN, HIGH);
      delay_ten_us(50000);         // 500 millisec delay
      //wdt_reset();                 // kick the dog
      // turn off visible LED at PB0 by pulling pin to +3V
      //PORTB |= _BV(LED);
      digitalWrite(LED_PIN, LOW);
      delay_ten_us(50000);         // 500 millisec delay
      //wdt_reset();                 // kick the dog
    }
}

