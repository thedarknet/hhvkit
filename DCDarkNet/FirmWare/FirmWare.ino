
#include <avr/pgmspace.h>
#include "IRSerial-2014.h"
//#include "MD5.h"
#include <EEPROM.h>
#include <avr/sleep.h>
#include <Wire.h>
#include <DarkNetDisplay.h>

// USB Keyboard port
//#define USB_PUBLIC static
#include "UsbKeyboard.h"

/*
 * Pinout:
 * 0, 1: FTDI serial header
 * 2, 4, 7: USB D+, D-, Pullup (respectively)
 * 8, 9: IR Rx, Tx (respectively)
 * 3: Morse LED
 * 5, 6, 10, 11: Backlight LEDs
 * 12: Button
 * 16: Display board UP button
 * 23: Display board RIGHT button
 * 24: Display board CENTER (ENTER) button
 * 25: Display Board DOWN button
 * 26: Display Board LEFT button
 * 27: SDA
 * 28: SCL
 */
#define MAKE_ME_A_CONTEST_RUNNER 0

// IR Parameters
#define IR_RX 8
#define IR_TX 9
#define IR_BAUD 300
IRSerial irSerial(IR_RX, IR_TX, false, true);

// Console port
#define SERIAL_BAUD 115200

// Main morse LED
#define LED_PIN 3

// Switch pin
#define BUTTON_PIN 12

#define BUTTON_UP        13 //which is the 16th counted pin //wont work bc also used for backlights ops!
#define BUTTON_LEFT      A3
#define BUTTON_RIGHT     A0
#define BUTTON_DOWN      A2
#define BUTTON_CENTER    A1
#define VUP (1<<0)
#define VDOWN (1<<1)
#define VLEFT (1<<2)
#define VRIGHT (1<<3)
#define VCENTER (1<<4)

// Backlight LEDs
#define BACKLIGHT_4 5
#define BACKLIGHT_3 6
#define BACKLIGHT_2 10
#define BACKLIGHT_1 11


//BEGIN SERIAL EPIC VARS
//bool to see if we have started the SerialEpic
#define MAX_SERIAL_EPIC_TIME_MS 30000
#define MAX_SERIAL_ANSWER_LENGTH 10


//END SERIAL EPIC VARS

// BEGIN STRINGS
//string that starts serial epic
static const char* START_SERIAL_EPIC_STRING = "JOSHUA";
//static const char* SERIAL_PORT_ANSWER = "42";
#define SERIAL_PORT_QUESTION F("What is the answer to all things?")
//length of serial string
#define MIN_SERIAL_LEN  6
#define SEND_TO_DAEMON F("Send this to daemon: ")
#define DisplayEpicText F("Who started the \nDC Dark Net?")
static const char* DisplayEpicAnswer = "SMITTY";
#define iKnowNot F("I know not what you speak of.")
#define sendTheCodes F("https://dcdark.net/  Send the following codes:\n")
static const char* const PROGMEM LINE1 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789!*#@%";
#define LINE1_LENGTH 42
#define EMPTY_STR F("")
#define DKN F("DKN-")
// END REPEATED STRINGS

//BEGIN GLOBAL VARS:
//We are getting tight on space so I'm packing variables as tightly as I can
struct _PackedVars {
  uint32_t InSerialEpic : 1;
  uint32_t AwaitingSerialAnswer: 1;
  uint32_t DisplayAnswer:1;
  uint32_t LEDMode : 4; //what start up mode are we need (animation)
  uint32_t Silent : 1;
  uint32_t LINE1Loc : 6;
  uint32_t AnswerPos : 4;
  uint32_t hasSilk1 : 1;
  uint32_t hasSilk2 : 1;
  uint32_t hasSilk3 : 1;
  uint32_t isContestRunner : 1;
  uint32_t swapKeyAndGUID : 1;
  uint32_t modeInit : 1;
} PackedVars;

uint16_t KEY[4];
char GUID[9];
unsigned long nextBeacon;
#define OLED_RESET 4
DarkNetDisplay Display(OLED_RESET);
//END GLOBAL VARS


//BEGIN MODE_DEFS
#define MODE_COUNT 9
#define MODE_MORSE_CODE_EPIC     0
#define MODE_SNORING             1
#define MODE_SERIAL_EPIC         2
#define MODE_DISPLAY_BOARD_EPIC  3
#define MODE_JUST_A_COOL_DISPLAY   4
#define MODE_SILK_SCREEN           5
#define MODE_UBER_BADGE_SYNC       6
#define MODE_BACKLIGHT             7
#define MODE_SHUTDOWN              8
//END MODE DEFS


// BEGIN EEPROM count location
#define MSG_COUNT_ADDR 1022
#define RESET_STATE_ADDR 1020
#define GUID_ADDR 1012
#define KEY_ADDR 1004

// Maximum number of messages
#define MAX_NUM_MSGS 60
#define GUID_SIZE 8
#define MORSE_CODE_ENCODED_MSG 8
#define TOTAL_STORAGE_SIZE_MSG (GUID_SIZE+MORSE_CODE_ENCODED_MSG)
#define MAX_MSG_ADDR                     (TOTAL_STORAGE_SIZE_MSG*MAX_NUM_MSGS) //960
#define START_EPIC_RUN_TIME_STORAGE       MAX_MSG_ADDR+20 //cushion 980
#define UBER_CRYTPO_CONTEST_RUNNER_ADDR   START_EPIC_RUN_TIME_STORAGE //first 8 bytes uber contest runner KEY then GUID
#define MAX_EPIC_RUN_TIME_STORAGE         START_EPIC_RUN_TIME_STORAGE+16

// END EEPROM COUNT LOCATION

#define ENDLINE F("\n")

// Morse Code constants
unsigned char const morse[28] PROGMEM = {
   0x05,   // A  .-     00000101
   0x18,   // B  -...   00011000
   0x1A,   // C  -.-.   00011010
   0x0C,   // D  -..    00001100
   0x02,   // E  .      00000010
   0x12,   // F  ..-.   00010010
   0x0E,   // G  --.    00001110
   0x10,   // H  ....   00010000
   0x04,   // I  ..     00000100
   0x17,   // J  .---   00010111
   0x0D,   // K  -.-    00001101
   0x14,   // L  .-..   00010100
   0x07,   // M  --     00000111
   0x06,   // N  -.     00000110
   0x0F,   // O  ---    00001111
   0x16,   // P  .--.   00010110
   0x1D,   // Q  --.-   00011101
   0x0A,   // R  .-.    00001010
   0x08,   // S  ...    00001000
   0x03,   // T  -      00000011
   0x09,   // U  ..-    00001001
   0x11,   // V  ...-   00010001
   0x0B,   // W  .--    00001011
   0x19,   // X  -..-   00011001
   0x1B,   // Y  -.--   00011011
   0x1C,   // Z  --..   00011100
   0x01,   // space     00000001
   0x5A,   // @  .--.-. 01011010
};

//debugging macros
//#define SERIAL_TRACE 
#ifdef SERIAL_TRACE
  #define SERIAL_TRACE_LN(a) Serial.println(a);
  #define SERIAL_TRACE(a) Serial.print(a);
#else
  #define SERIAL_TRACE_LN(a)
  #define SERIAL_TRACE(a)
#endif

//#define SERIAL_INFO 
#ifdef SERIAL_INFO
  #define SERIAL_INFO_LN(a) Serial.println(a);
  #define SERIAL_INFO(a) Serial.print(a);
#else
  #define SERIAL_INFO_LN(a)
  #define SERIAL_INFO(a)
#endif
//end debugging macros


void readContestRunnerKeyAndGUID() {
#if MAKE_ME_A_CONTEST_RUNNER //make contest runner badge
  // Pull GUID and Private key from EEPROM
  for (uchar ndx = 0; ndx < 8; ndx++) {
    GUID[ndx] = EEPROM.read(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 8 + ndx);
  }
  GUID[8] = 0;
  // Yes, this is big endian. I don't want to have to byte-swap
  // when building the EEPROM file from text strings.
  KEY[0] = (EEPROM.read(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 0) << 8) + EEPROM.read(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 1);
  KEY[1] = (EEPROM.read(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 2) << 8) + EEPROM.read(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 3);
  KEY[2] = (EEPROM.read(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 4) << 8) + EEPROM.read(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 5);
  KEY[3] = (EEPROM.read(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 6) << 8) + EEPROM.read(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 7);
#endif
}

void readInGUIDAndKey() {
  for (uchar ndx = 0; ndx < 8; ndx++) {
    GUID[ndx] = EEPROM.read(GUID_ADDR + ndx);
  }
  GUID[8] = 0;
  // Yes, this is big endian. I don't want to have to byte-swap
  // when building the EEPROM file from text strings.
  KEY[0] = (EEPROM.read(KEY_ADDR + 0) << 8) + EEPROM.read(KEY_ADDR + 1);
  KEY[1] = (EEPROM.read(KEY_ADDR + 2) << 8) + EEPROM.read(KEY_ADDR + 3);
  KEY[2] = (EEPROM.read(KEY_ADDR + 4) << 8) + EEPROM.read(KEY_ADDR + 5);
  KEY[3] = (EEPROM.read(KEY_ADDR + 6) << 8) + EEPROM.read(KEY_ADDR + 7);
}

/* This is the workhorse function.  Whatever you do elsewhere,
 * When you're not working you need to call this so it looks
 * for IR data and acts on it if received.  The Rx ISR has a
 * 64 byte buffer, so you need to call this at least once every
 * (1 sec/300 symbols * 10 symbos/1 byte * 64 byte buffer = )
 * 2.13 seconds before you risk overflowing the buffer, but I'd
 * make sure you call it more often than that.  Basically,
 * whenever you are pausing, use this instead of delay().
 *
 * Like delay(), but looks for IR serial data while waiting.
 * Delays for pauseFor milliseconds.  Tight loops around looking
 * for serial data.  Also triggers a USB dump if the button is
 * pressed.
 * 
 * IMPORTANT: If either a full Serial frame is received, or the
 * button is pressed, then delayAndReadIR() can delay for much
 * longer than pauseFor milliseconds as it either handles the USB,
 * or handles a received IR signal.
 */
void delayAndReadIR(int pauseFor) {
  pauseFor = pauseFor>0?pauseFor:0;
  uint32_t returnTime = millis() + pauseFor;
  while (millis() < returnTime) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      long buttonStart = millis();
      while(digitalRead(BUTTON_PIN) == LOW) {
        if (millis() - buttonStart > 2000) {
          buttonStart = 0;  // Flag to not dump USB
          break;
        }
      }
      if (buttonStart) {
        // Dump database to USB
        dumpDatabaseToUSB();
      }
    }
    int ret;
    if (ret = irSerial.available()) {
      processIR(irSerial.read());
    }
  }
}

// Sends our GUID out over the IR.  Call this "often" 
// when it's convenient in your animation. It'll handle
// dealing with the timing, whether it's been long enough
// to send another beacon.
void beaconGUID(void) {
  if (millis() >= nextBeacon) {
    if(!PackedVars.Silent) {
      Serial.println(F("Ping!"));
    }
    if (1 == PackedVars.swapKeyAndGUID) {
      readContestRunnerKeyAndGUID();
    }
    // Add a little randomness so devices don't get sync'd up.
    // Will beacon on average every 5 seconds.
    nextBeacon = millis() + random(4000, 6000);
    irSerial.print(F("0x"));
    irSerial.println(GUID);
  }
}

// The length of one morse code symbol (one "dit", and 
// the spaces between dits and dashes.)
void MorsePause(int ndx) {
  delayAndReadIR(ndx*250);
}

// Blink a character in morse code.
void sendMorse(uint8_t chr) {
  uint8_t bit;
  
  if (chr == ' ')
    chr = pgm_read_byte(morse + 26);
  else if (chr == '@')
    chr = pgm_read_byte(morse + 27);
  else if (chr >= 'A' && chr <= 'Z')
    chr = pgm_read_byte(morse + (chr-'A'));
  else {
    digitalWrite(LED_PIN, HIGH);
    MorsePause(15);
    digitalWrite(LED_PIN, LOW);
    return;
  }
  
  // Get rid of padding, looking for the start bit.
  for (bit = 0; bit < 8 && (chr & 0x80) == 0; bit++, chr<<=1)
    ;
    
  // Pass the start bit, then send the rest of the char in morse
  for (bit++, chr<<=1; bit < 8; bit++, chr<<=1) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BACKLIGHT_1, HIGH);
    if (chr & 0x80) {
      digitalWrite(BACKLIGHT_2, HIGH);
      digitalWrite(BACKLIGHT_3, HIGH);
      digitalWrite(BACKLIGHT_4, HIGH);
      MorsePause(3);
    } else {
      MorsePause(1);
    }
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BACKLIGHT_1, LOW);
    digitalWrite(BACKLIGHT_2, LOW);
    digitalWrite(BACKLIGHT_3, LOW);
    digitalWrite(BACKLIGHT_4, LOW);
    MorsePause(1);
  }
  MorsePause(4);  // Inter character pause
}

void sendMorseStr(const __FlashStringHelper *str) {
  PGM_P p = reinterpret_cast<PGM_P>(str);
  while (1) {
    unsigned char c = pgm_read_byte(p++);
    if(c==0) break;
    sendMorse(c);
    beaconGUID();
  }
}

void sendMorseStr(const char *msg) {
  for (; *msg != '\0'; msg++) {
    sendMorse(*msg);
    beaconGUID();
  }
}

// LULZ Twitter. That's old school...  Haven't used Twitter for this since 2012.
void sendMorseTwitter(uint16_t msgAddr) {
  sendMorseStr(F("DKN DASH "));
  for (unsigned char ndx = 0; ndx < 8; ndx++) {
    sendMorse(EEPROM.read(msgAddr+ndx));
  }
}

void sendSerialTwitter(uint16_t msgAddr) {
  Serial.print(DKN);
  for (unsigned char ndx = 0; ndx < 16; ndx++) {
    Serial.write(EEPROM.read(msgAddr+ndx));
  }
  Serial.println(EMPTY_STR);
}
  
// TEA (known to be broken) with all word sizes cut in half (64 bit key, 32 bit blocks)
// Yes, I'm inviting people to hack this if they want. :-)
// TODO: Since we're giving up on backward compatability in 2014, should we increase this size?
void encrypt(uint16_t *v) {
  uint16_t v0=v[0], v1=v[1], sum=0, i;           // set up
  uint16_t delta=0x9e37;                         // a key schedule constant
  for (i=0; i < 32; i++) {                       // basic cycle start
    sum += delta;
    v0 += ((v1<<4) + KEY[0]) ^ (v1 + sum) ^ ((v1>>5) + KEY[1]);
    v1 += ((v0<<4) + KEY[2]) ^ (v0 + sum) ^ ((v0>>5) + KEY[3]);  
  }                                              /* end cycle */
  v[0]=v0; v[1]=v1;
}

uint16_t getNumMsgs() {
  return (EEPROM.read(MSG_COUNT_ADDR)<<8) + EEPROM.read(MSG_COUNT_ADDR+1);
}

void writeNumMsgs(uint16_t numMsgs) {
  EEPROM.write(MSG_COUNT_ADDR, numMsgs/256);
  EEPROM.write(MSG_COUNT_ADDR+1, numMsgs%256);
}  

bool isNumMsgsValid(uint16_t numMsgs) {
  return numMsgs < MAX_NUM_MSGS;
}

// Writes a pairing code to EEPROM
int writeEEPROM(unsigned char *guid, uint8_t *msg) {
  char msgStr[9];
  intToStr(msg, msgStr);
  
  uint16_t numMsgs = getNumMsgs();
  if (!isNumMsgsValid(numMsgs)) {
    Serial.print(F("numMsgs not valid. Setting to 0. Ignore if this is your first pairing."));
    Serial.println(numMsgs);
    // Assume this is the first read of this chip and initialize
    numMsgs = 0;
  }
  
  int msgAddr;
  unsigned char ndx;
  for (msgAddr = 0; msgAddr < numMsgs*TOTAL_STORAGE_SIZE_MSG; msgAddr+=TOTAL_STORAGE_SIZE_MSG) {
    for (ndx = 0; ndx < 8; ndx++) {
      if (guid[ndx] != EEPROM.read(msgAddr+ndx))
        break;
    }
    if (ndx == 8) {
      // Found a match.  Rewrite in case it was wrong before
      break;
    }
  }

  if (ndx != 8) {  
    // FIXME This doens't properly stop at 60. It wraps around.
    if (numMsgs > 0 && !isNumMsgsValid(numMsgs)) {
      // The DB is full.  Don't actually create a new entry.
      for (ndx = 0; ndx < 16; ndx++) {
        // Randy Quaid is my hero.
        EEPROM.write(msgAddr+ndx, "SHITTERS FULL..."[ndx]);
      }
      return msgAddr;
    } 
    else {
      numMsgs++;
    }
  }
  
  for (ndx = 0; ndx < 8; ndx++) {
    EEPROM.write(msgAddr+ndx, guid[ndx]);
    EEPROM.write(msgAddr+8+ndx, msgStr[ndx]);
  }
  writeNumMsgs(numMsgs);
  return msgAddr;
}
  

// This is our receive buffer, for data we pull out of the
// IRSerial library.  This is _NOT_ the buffer the IRSerial
// library is using to receive bytes into.
// Our buffer only needs to be big enough to hold a staza
// to process.  Right now (2014) that's 12 bytes.  I'm over-sizing
// 'cuz why not?
#define RX_BUF_SIZE 32
unsigned char rxBuf[RX_BUF_SIZE];
unsigned char head;

// Increments a pointer into rxBuf, wrapping as appropriate.
unsigned char inc(unsigned char *p) {
  unsigned char q = *p;
  *p = (*p+1) % RX_BUF_SIZE;
  return q;
}

// Returns the character in rxBuf[] that is p bytes offset from head.
// Deals with wrapping.  You'll probably want to pass a negative
// number as p.
unsigned char rxBufNdx(unsigned char p) {
  return rxBuf[(head+p)%RX_BUF_SIZE];
}

// Turns a 4 byte int (represented here as an array of bytes)
// into a "modified HEX" string.  tr/0-9a-f/A-P/   Makes it
// easier to send in morse code.
void intToStr(uint8_t *src, char *dst) {
  for (unsigned char ndx = 0; ndx < 8; ndx++) {
    dst[ndx] = ((src[ndx>>1] >> (ndx%2?0:4)) & 0x0F) + 'A';
  }
  dst[8] = '\0';
}

void writeWord(uint8_t *buf) {
  char str[9];
  intToStr(buf, str);
  irSerial.println(str);
}

/* Processes the received buffer, pulls the 8 character "modified HEX"
 * out of rxBuf[] (head points at the next byte after the end) and packs it into a 
 * 4 byte array *packedBuf.  If provided, the original 8 "hex" characters 
 * are also copied into *strDst.
 * Messages are of the form:   m/(0[xyab])([A-Z0-9]{8})\r\n/
 * $1 is the header and specifies which message it is.  
 *   0x = Alice's GUID beacon  (Alice -> Bob)
 *   0y = Bob's encrypted reply to Alice's GUID beacon  (Bob -> Alice)
 *   0a = Bob's GUID (Bob -> Alice, sent immediately after 0y)
 *   0b = Alice's encrypted reply to Bob's GUID  (Alice -> Bob)
 *   0w = Message from DarkNet Agent after you've solved the 6 part silk screen crypto
 */
unsigned char readWordFromBuf(uint8_t *packedBuf, unsigned char *strDst = 0) {
  // head points to the next character after the \r\n at the end
  // of our received bytes.  So head-10 points to the beginning of
  // our message, but it's a circular buffer, so we have to wrap.
  unsigned char rxNdx = (head-10)%RX_BUF_SIZE;
  
  for (unsigned char ndx = 0; ndx < 8; ndx++, inc(&rxNdx)) { 
    unsigned char packedPtr = ndx >> 1;  // index into *packedBuf
    unsigned char cur = rxBuf[rxNdx]; // current "HEX" character
    
    // Convert from our modified HEX into the actual nibble value.
    if (cur >= 'A' && cur <= 'P') {
      cur -= 'A';
    } else if (cur >= 'Q' && cur <= 'Z') {
      cur -= 'Q';
    } else if (cur >= '0' && cur <= '9') {
      cur = cur - '0' + 6;
    } else {
      Serial.print(F("readWordFromBuf() line noise: "));
      Serial.println(cur);
      return 0;  // Line noise.  Return and wait for the next one.
    }      
    packedBuf[packedPtr] <<= 4;  // Shift up the previous nibble, filling with zeros
    packedBuf[packedPtr] |= (cur & 0x0F); // Add in the current nibble
    
    // If provided, also copy rxBuf into *strDst.
    if (strDst) {  
      *(strDst++) = rxBuf[rxNdx];
    }
  }
  return 1;
}

unsigned char isValidWord() {
  // Check for valid framing.
  if (rxBufNdx(-1) != '\n' || rxBufNdx(-2) != '\r' || rxBufNdx(-12) != '0') {
    // Probably in the middle of receiving, nothing wrong.
    // Don't log anything, just return.  
    return false;
  }
  
  // We have a good framing. Future failures will be reported.
  char c = rxBufNdx(-11);
  if (c!='x' && c!='y' && c!='a' && c!='b') {
    Serial.println(F("Bad rx header: "));
    Serial.println(c);
    return false;
  }
  for (int i = -10; i < -2; i++) {
    c = rxBufNdx(i);
    
    // If it's not a letter and not a number
    if (!(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9')) {  
      Serial.println(F("Bad rx data: "));
      Serial.println(i);
      Serial.println(F(" "));
      Serial.println(c);
      return false;
    }
  }
  return true;
}

// Reads more characters from the IR and processes them as they come in.
// This differs from processIR() below in that it waits 5000ms for a message
// to come in, rather than returning immediately.  So only call this if
// you're already in the middle of an exchange and know you want to wait.
unsigned char readWordFromIR() {
  head = 0;
  unsigned long start = millis();
  while (millis() - start < 5000) {  // Loop for no more than 5 seconds
    if (!irSerial.available())
      continue;
    unsigned char c = irSerial.read();
    rxBuf[head] = c;
    head = (head+1) % RX_BUF_SIZE;
    if (isValidWord())
      return 1;
  }
  return 0;
}

int weAreAlice() {
  digitalWrite(BACKLIGHT_1, HIGH);
  // Read the 0y from Bob and process it.
  uint8_t aliceEnc[4] = {0, 0, 0, 0};
  uint8_t bob[4] = {0, 0, 0, 0};
  unsigned char bobGUID[8];
  if (!readWordFromBuf(aliceEnc)) {
    Serial.println(F("Error reading 0y from rxBuf."));
    return -1;
  }
  digitalWrite(BACKLIGHT_2, HIGH);

  // Right on the heels is 0a, Bob's GUID
  if (!readWordFromIR() || rxBufNdx(-11) != 'a') {
    Serial.println(F("Error reading 0a"));
    return -1;
  }
  digitalWrite(BACKLIGHT_3, HIGH);

  if (!readWordFromBuf(bob, bobGUID)) {
    Serial.println(F("Error reading 0a from rxBuf"));
    return -1;
  }
  
  uint8_t bobEnc[4];
  // Nasty pointer math to convert to the right format for encryption.
  // TODO this could almost certainly be done cleaner.
  *(uint32_t*)bobEnc = *(uint32_t*)bob;
  encrypt((uint16_t*)bobEnc);
  delay(100);  // Give Bob some time to recover from his send
  irSerial.print(F("0b"));
  writeWord(bobEnc);
  digitalWrite(BACKLIGHT_4, HIGH);
  
  // Alright!  We've got everything we need!  Build a message
  *(uint32_t*)bobEnc ^= *(uint32_t*)aliceEnc;
  int retVal =  writeEEPROM(bobGUID, bobEnc);

  if (1 == PackedVars.swapKeyAndGUID) {
    PackedVars.swapKeyAndGUID = 0;
    readInGUIDAndKey();
  }
  return retVal;
}

int weAreBob() {
  // Read the 0x from Alice and process it.
  digitalWrite(BACKLIGHT_4, HIGH);
  uint8_t alice[4] = {0, 0, 0, 0};
  unsigned char aliceGUID[8];
  if (!readWordFromBuf(alice, aliceGUID)) {
    Serial.println(F("Error reading 0x from rxBuf."));
    return -1;
  }
  digitalWrite(BACKLIGHT_3, HIGH);

  uint8_t aliceEnc[4];
  *(uint32_t*)aliceEnc = *(uint32_t*)alice;
  encrypt((uint16_t*)aliceEnc);
  
  // Transmit response, plus my GUID
  delay(100);  // Sleep a little bit to let the other side clear out their buffer. ??
  irSerial.print(F("0y"));
  writeWord(aliceEnc);
  delay(100);  // Give the other side a bit of time to process.
  irSerial.print(F("0a"));
  irSerial.println(GUID);

  // Look for a response from Alice
  if (!readWordFromIR() || rxBufNdx(-11) != 'b') {
    Serial.println(F("Error reading 0b"));
    return -1;
  }
  digitalWrite(BACKLIGHT_2, HIGH);
    
  uint8_t bobEnc[4] = {0, 0, 0, 0};
  if (!readWordFromBuf(bobEnc)) {
    Serial.println(F("Error reading 0b from rxBuf."));
    return -1;
  }
  digitalWrite(BACKLIGHT_1, HIGH);
  
  // Alright!  We've got everything we need!  Build a message
  *(uint32_t*)bobEnc ^= *(uint32_t*)aliceEnc;
  return writeEEPROM(aliceGUID, bobEnc);
}

void clearRxBuf() {
  for (int ndx = 0; ndx < RX_BUF_SIZE; ndx++)
    rxBuf[ndx] = '-';
  head = 0;
}

/* The IRSerial library feeds us characters as they're received.
 * processIR() puts the character on the buffer and sees
 * if we have a properly formatted IR message. If we
 * do, it kicks off the appropriate process, whether we are
 * Alice or Bob.  If it doesn't it returns quickly so you can
 * be doing other things (like LED animation!)
 */
void processIR(unsigned char c) {
  //Serial.write(c);
  //Serial.print(" ");
  //Serial.print(c);
  //Serial.println("");
  rxBuf[head] = c;
  head = (head+1) % RX_BUF_SIZE;

  // isValidWord() will print an error message if it finds
  // a good header, but an otherwise malformed packet.
  // Otherwise, a false return code just means it didn't 
  // find a valid packet header, we're probably in the middle
  // of receiving a packet.
  if (!isValidWord())
    return;
    
  // Signal that we are receiving a packet and attempting an exchange
  analogWrite(LED_PIN, 32);
  digitalWrite(BACKLIGHT_1, LOW);
  digitalWrite(BACKLIGHT_2, LOW);
  digitalWrite(BACKLIGHT_3, LOW);
  digitalWrite(BACKLIGHT_4, LOW);
  
  unsigned char flag = rxBufNdx(-11);
  int msgAddr = -1;
  if (flag == 'x') {
    // 0x means we heard someone else's beacon,
    //Serial.println(F("Bob"));
    msgAddr = weAreBob();
  } else if (flag == 'y') {
    // 0y means someone else is responding to our beacon.
    //Serial.println(F("Alice"));
    msgAddr = weAreAlice();
  }
  
  unsigned char ndx;
  if (msgAddr == -1) {
    // Failed
    for (ndx = 0; ndx < 4; ndx++) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
    // Clean up
    delay(1000);
    clearRxBuf();
    digitalWrite(BACKLIGHT_1, LOW);
    digitalWrite(BACKLIGHT_2, LOW);
    digitalWrite(BACKLIGHT_3, LOW);
    digitalWrite(BACKLIGHT_4, LOW);

    return;
  }
  // Success!
  for (ndx = 0; ndx < 4; ndx++) {
    digitalWrite(LED_PIN, HIGH);
    delay(300);
    analogWrite(LED_PIN, 32);
    delay(300);
  }
  digitalWrite(LED_PIN, LOW);
  
  // Clean up
  delay(1000);
  clearRxBuf();
  digitalWrite(BACKLIGHT_1, LOW);
  digitalWrite(BACKLIGHT_2, LOW);
  digitalWrite(BACKLIGHT_3, LOW);
  digitalWrite(BACKLIGHT_4, LOW);

  sendSerialTwitter(msgAddr);
  sendMorseTwitter(msgAddr);
}

void dumpDatabaseToSerial() {
  uint16_t numMsgs = getNumMsgs();
  Serial.print(F("# of messages: "));
  Serial.println(numMsgs);
  Serial.print(sendTheCodes);
  Serial.print(F("HHVSERIAL-"));
  Serial.println(GUID);
  
  if (isNumMsgsValid(numMsgs)) {
    for (int msgAddr = 0; msgAddr < numMsgs*16; msgAddr+=16)
      sendSerialTwitter(msgAddr);
    Serial.println(EMPTY_STR);
  }
}

UsbKeyboardDevice *UsbKeyboard;

// Returns true if it finds a USB host, false if
// the timer expired before it could find one.
#define USB_UPDATE_EVERY 15
bool waitForHost(long ms) {
  SERIAL_TRACE_LN("waitForHost start");
  for (; ms > USB_UPDATE_EVERY; ms-=USB_UPDATE_EVERY) {
    delay(USB_UPDATE_EVERY);
    UsbKeyboard->update();
    if (usbInterruptIsReady()) {
      return true;
    }
  }
  delay(ms);
  SERIAL_TRACE_LN("waitForHost failed");
  return usbInterruptIsReady();
}

#define USB_DELAY_INTERVAL 25
void usbDelay(long ms) {
  SERIAL_TRACE_LN("usbdelay start");
  for (; ms > USB_DELAY_INTERVAL; ms-=USB_DELAY_INTERVAL) {
    delay(USB_DELAY_INTERVAL);
    UsbKeyboard->update();
  }
  delay(ms);
  SERIAL_TRACE_LN("usbdelay end");
}


// Setus up a USB keyboard, outputs our database to it, then
// disconnects again.  This way, we don't have to service USB
// routines while doing everything else.
void dumpDatabaseToUSB() {
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(BACKLIGHT_1, LOW);
  digitalWrite(BACKLIGHT_2, LOW);
  digitalWrite(BACKLIGHT_3, LOW);
  digitalWrite(BACKLIGHT_4, LOW);

//Serial.println("before");
  UsbKeyboard = new UsbKeyboardDevice();
//Serial.println("after");
 //Serial.println((int)UsbKeyboard,HEX);
  
  usbDelay(5000);
  // Give the USB a few seconds to settle in and be detected by the OS.
  //if (waitForHost(10000)) {
  if(usbInterruptIsReady()) {
    Serial.println(F("USB is ready."));
    usbDelay(100);
    SERIAL_TRACE_LN("back from delay");
    printUSB(sendTheCodes);
    printUSB(F("HHVUSB-"));
    printUSB(GUID);
    printUSB(ENDLINE);
    //Serial.println("back from printing");
    uint16_t numMsgs = getNumMsgs();
    if (isNumMsgsValid(numMsgs)) {
      for (int msgAddr = 0; msgAddr < numMsgs*16; msgAddr+=16) {
        Serial.println(F("."));
        //DAC - adding because if there are enough messages the usb will lock up because update has not been called.
        UsbKeyboard->update();
        sendUSBTwitter(msgAddr);
      }
      printUSB(ENDLINE);
    } 
    // Give the USB a second to finalize the last characters.
    usbDelay(1000);
  } else {
      Serial.println(F("USB Connect Failed"));
      //TODO
      //Should we flash LED PIN for a few seconds 
  }  
  
  // Clean up the USB and shut it down.
  usbDeviceDisconnect();
  delete UsbKeyboard;
  UsbKeyboard = 0;
  digitalWrite(LED_PIN, LOW);
}

void sendUSBTwitter(int msgAddr) {
  printUSB(DKN);
  uint8_t p;
  for (unsigned char ndx = 0; ndx < 16; ndx++) {
    p = EEPROM.read(msgAddr+ndx);
    writeUSB(p);
  }
  printUSB(ENDLINE);
}

// copied from Print.cpp
void printUSB(const __FlashStringHelper *str) {
  PGM_P p = reinterpret_cast<PGM_P>(str);
  while (1) {
    unsigned char c = pgm_read_byte(p++);
    if(c==0) break;
    writeUSB(c);
  }
}

void printUSB(const char *str) {
  // LULZ BOUNDZ CHEX!!!!111one
  while(*str != 0) {
    writeUSB(*str);
    str++;
  }
}

void writeUSB(char c) {
  //Serial.println(c);
  //UsbKeyboard->update();
  
  // This if() block converts from ASCII to scan-codes.
  // There's probably a better way to do this.  I suspect
  // Putting all the single character if() blocks into a 
  // single case statement might be good.  Dunno how smart
  // the compiler is though.
  
  // USB scancodes can be found here: http://www.win.tue.nl/~aeb/linux/kbd/scancodes-14.html
  if (c >= 'a' && c <= 'z') {  // a through z start at scan code 4
    UsbKeyboard->sendKeyStroke(c - 'a' + 4, 0);
  } else if (c >= 'A' && c <= 'Z') { // same as a through z, but holding down shift.
    UsbKeyboard->sendKeyStroke(c - 'A' + 4, MOD_SHIFT_LEFT);
  } else if (c >= '1' && c <= '9') { // 1 through 9 start at 30, with 0 at the end
    UsbKeyboard->sendKeyStroke(c - '1' + 30);
  } else if (c == '0') {
    UsbKeyboard->sendKeyStroke(39);
  } else if (c == ':') {
    UsbKeyboard->sendKeyStroke(51, MOD_SHIFT_LEFT);
  } else if (c == '/') {
    UsbKeyboard->sendKeyStroke(56);
  } else if (c == '.') {
    UsbKeyboard->sendKeyStroke(55);
  } else if (c == '-') {
    UsbKeyboard->sendKeyStroke(45);
  } else if (c == ' ') {
    UsbKeyboard->sendKeyStroke(KEY_SPACE);
  } else if (c == '\n') {
    UsbKeyboard->sendKeyStroke(KEY_ENTER);
  } // TODO If you use any other characters in dumpDatabase, define them here.
  else { // ERROR
    SERIAL_TRACE("ERROR: Unknown character: ");
    SERIAL_TRACE(int(c));
    SERIAL_TRACE(" ");
    SERIAL_TRACE(c);
    SERIAL_TRACE_LN("");
  }
}

void BlinkMainLEDForMode(int mode) {
  for(int i=0;i<mode;i++) {
    digitalWrite(LED_PIN, HIGH);
    delayAndReadIR(400);
    digitalWrite(LED_PIN, LOW);
    delayAndReadIR(400);
  }
  analogWrite(LED_PIN, 16);
  delayAndReadIR(100);
  analogWrite(LED_PIN, 32);
  delayAndReadIR(100);
  analogWrite(LED_PIN, 64);
  delayAndReadIR(100);
  analogWrite(LED_PIN, 128);
  delayAndReadIR(100);
  analogWrite(LED_PIN, 255);
  delayAndReadIR(100);
  digitalWrite(LED_PIN, LOW);
}

void setup() {
  // Setup various serial ports
  // Serial is used as a console.  It shows up on both the FTDI header
  // and ICSP.  You can also reprogram the board as if it were an Arduino
  // through the FTDI.  :-)
  Serial.begin(SERIAL_BAUD);
  // A modified version of SoftwareSerial to handle inverted logic
  // (Async serial normally idles in the HIGH state, which would burn 
  // through battery on our IR, so inverted logic idles in the LOW state.)
  // Also modified to modulate output at 38kHz instead of just turning the
  // LED on.  Otherwise, it's a pretty standard SoftwareSerial library.
  irSerial.begin(IR_BAUD);  
  irSerial.listen();
  // For some reason, the TX line starts high and wastes some battery.
  digitalWrite(IR_TX, LOW);
  
  // Setup LED outputs.  This is the single Green LED that everyone gets.
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // These are the "DC  DN" back-lit letters that only Ubers get. :-)
  //                12  34
  // (If you add your own LEDs, you are by definition Uber so I can say that. :-)
  pinMode(BACKLIGHT_1, OUTPUT);
  digitalWrite(BACKLIGHT_1, LOW);
  pinMode(BACKLIGHT_2, OUTPUT);  
  digitalWrite(BACKLIGHT_2, LOW);
  pinMode(BACKLIGHT_3, OUTPUT);
  digitalWrite(BACKLIGHT_3, LOW);
  pinMode(BACKLIGHT_4, OUTPUT);
  digitalWrite(BACKLIGHT_4, LOW);
  
  // Setup the USB TX button.
  pinMode(BUTTON_PIN, INPUT);
  digitalWrite(BUTTON_PIN, HIGH);  // Internal pull-up
  
  //setup display board buttons
  pinMode(BUTTON_UP, INPUT);
  digitalWrite(BUTTON_UP, HIGH);  // Internal pull-up
  
  delay(200);  // Reset "debounce"

  // Setup the IR buffers and timers.
  nextBeacon = millis();  
  clearRxBuf();
  

  readInGUIDAndKey();  
  
#if MAKE_ME_A_CONTEST_RUNNER //make contest runner badge
  Serial.println(F("making badge contest runner"));
  PackedVars.isContestRunner = 1;

  //complete uber silk screen epic
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 0 , 'K');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 1 , 'R');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 2 , 'U');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 3 , 'X');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 4 , 'C');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 5 , 'O');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 6 , 'D');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 7 , 'E');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 8 , 'D');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 9 , 'E');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 10, 'A');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 11, 'D');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 12, '0');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 13, '9');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 14, '1');
  EEPROM.write(UBER_CRYTPO_CONTEST_RUNNER_ADDR + 15, '1');
#else
  PackedVars.isContestRunner = 0;
#endif

  
  //set initial Pack Vars
  PackedVars.InSerialEpic = 0; // no serial received yet so we are not on serial epic
  PackedVars.AwaitingSerialAnswer = 0; //no serial epic active yet so we don't have a state for it
  PackedVars.Silent = 0;
  PackedVars.LINE1Loc = 0;
  PackedVars.AnswerPos = 0;
  PackedVars.DisplayAnswer = 0;
  PackedVars.hasSilk1 = 0;
  PackedVars.hasSilk2 = 0;
  PackedVars.hasSilk3 = 0;
  PackedVars.swapKeyAndGUID = 0;
  PackedVars.modeInit = 0;
  
  // BLINKY SHINY!  
  PackedVars.LEDMode = EEPROM.read(RESET_STATE_ADDR)%MODE_COUNT;
  EEPROM.write(RESET_STATE_ADDR, (PackedVars.LEDMode + 1)%MODE_COUNT); 
 
  
  #if 0
    PackedVars.LEDMode = MODE_DISPLAY_BOARD_EPIC;
    Serial.print(F("is contest runner: "));
    Serial.println(PackedVars.isContestRunner);
    //Serial.println(sizeof(UsbKeyboard));
    //writeNumMsgs(0);
  #endif

  BlinkMainLEDForMode(PackedVars.LEDMode);
  if (PackedVars.LEDMode == MODE_MORSE_CODE_EPIC) {        // Normal Morse code
    Serial.println(F("Morse output of codes...0"));
    sendMorse(LINE1[4]); //E
  } else if (PackedVars.LEDMode == MODE_SNORING) { // Snoring
    Serial.println(F("Snoring...1"));
    sendMorse(LINE1[8]); // I
  } else if (PackedVars.LEDMode == MODE_BACKLIGHT ) {
    Serial.println(F("BackLight...7"));
    sendMorse(LINE1[18]); // S
  } else if (PackedVars.LEDMode == MODE_SERIAL_EPIC) { //epic
    Serial.println(F("Operative...2"));
    sendMorse(LINE1[19]);  //T
  } else if (PackedVars.LEDMode == MODE_DISPLAY_BOARD_EPIC) {
    Serial.println(F("Uber Operative...3"));
    sendMorse(LINE1[3]); //D
    Display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
    Display.display();
    delayAndReadIR(3000);
    Display.clearDisplay();
  } else if (PackedVars.LEDMode == MODE_JUST_A_COOL_DISPLAY) {
    Serial.println(F("Life!...4"));
    if(0==PackedVars.modeInit) {
      int seedNum = KEY[0]<<24 | KEY[1]<<16 | KEY[2] << 8 | KEY[3];
      srand(seedNum);
      PackedVars.modeInit = 1;
    }
    
    sendMorse(LINE1[0]); //A
    Display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
    Display.display();
    delayAndReadIR(3000);
    Display.clearDisplay();
  } else if (PackedVars.LEDMode == MODE_SILK_SCREEN) {
    Serial.println(F("Operative: Crypto...5"));
    sendMorse(LINE1[1]); //B
  } else if (PackedVars.LEDMode == MODE_UBER_BADGE_SYNC) {
    Serial.println(F("Uber Operative: Crypto...6"));
    sendMorse(LINE1[2]); //C
    Display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
    Display.display();
    delayAndReadIR(3000);
    Display.clearDisplay();
  } else { //if (PackedVars.LEDMode == MODE_SHUTDOWN) { // Off
    Serial.println(F("Shutting down...8"));
    //sendMorse('M');          // --
    // Except, ya know, do it while not listening for IR.
    digitalWrite(LED_PIN, HIGH);
    delay(750);
    digitalWrite(LED_PIN, LOW);
    delay(250);
    digitalWrite(LED_PIN, HIGH);
    delay(750);
    digitalWrite(LED_PIN, LOW);
    delay(250);
    
    // Shutdown our outputs to save battery.
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BACKLIGHT_1, LOW);
    digitalWrite(BACKLIGHT_2, LOW);
    digitalWrite(BACKLIGHT_3, LOW);
    digitalWrite(BACKLIGHT_4, LOW);
    digitalWrite(BUTTON_PIN, LOW);
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    sleep_mode();            // Won't wake up until reset.
  }

  // 'cuz why not?
  delay(100);
}

// Takes LEDs from Start to End in steps*pause milliseconds
void rampLED(uchar ledStart, uchar ledEnd, 
 uchar back1Start, uchar back1End,
 uchar back2Start, uchar back2End,
 uchar back3Start, uchar back3End,
 uchar back4Start, uchar back4End,
 int rampTime, int stepTime = 10) {
  long startTime = millis(); 
  long endTime = startTime + rampTime;
  
  uint8_t ledDiff = ledEnd-ledStart;
  uint8_t back1Diff = back1End-back1Start;
  uint8_t back2Diff = back2End-back2Start;
  uint8_t back3Diff = back3End-back3Start;
  uint8_t back4Diff = back4End-back4Start;
  //Serial.println("before while end time");
  //Serial.println(endTime);
  while(millis() < endTime - stepTime) {
    long curTime = millis() - startTime;
    analogWrite(LED_PIN, ledStart+(ledDiff*curTime)/rampTime);
    analogWrite(BACKLIGHT_1, back1Start+(back1Diff*curTime)/rampTime);
    analogWrite(BACKLIGHT_2, back2Start+(back2Diff*curTime)/rampTime);
    analogWrite(BACKLIGHT_3, back3Start+(back3Diff*curTime)/rampTime);
    analogWrite(BACKLIGHT_4, back4Start+(back4Diff*curTime)/rampTime);
    delayAndReadIR(stepTime);
  }
  //Serial.println("after while");
  analogWrite(LED_PIN, ledEnd);
  analogWrite(BACKLIGHT_1, back1End);
  analogWrite(BACKLIGHT_2, back2End);
  analogWrite(BACKLIGHT_3, back3End);
  analogWrite(BACKLIGHT_4, back4End);
  delayAndReadIR(endTime-millis());
}

void GenerateResponseToCorrectEpic(const char *answer, boolean onDisplay) {
  unsigned char Result[18] = {0};
  memcpy(&Result[0],answer,2);
  memcpy(&Result[2],&KEY[0],8);
  memcpy(&Result[10],&GUID[0],8);

#if 0
  Serial.println(F("18 byte vector before encrypt"));
  for(int i=0;i<sizeof(Result);i++) {
    Serial.print(Result[i],HEX);
  }
  Serial.println(EMPTY_STR);
  Serial.println(F("Key HEX"));
  for(int i=0;i<4;i++) {
    Serial.print(KEY[i],HEX);
  }
  Serial.println(EMPTY_STR);
  Serial.println(F("Key shorts"));
  for(int i=0;i<4;i++) {
    Serial.print(KEY[i]);
    Serial.print(" ");
  }
  Serial.println(EMPTY_STR);
  Serial.println(F("GUID"));
  Serial.println(&GUID[0]);
#endif

  for(int i=0;i<(sizeof(Result)-4);i+=2) {
    encrypt((uint16_t*)&Result[i]);  
  }
  
  Serial.println(SEND_TO_DAEMON);
  if(onDisplay) Display.println(SEND_TO_DAEMON);
  for(int i=0;i<sizeof(Result);i++) {
    if(Result[i]<16) { //otherwise we'll get just something like 6 rather than 06
      Serial.print("0");
    }
    Serial.print(Result[i],HEX);
    if(onDisplay) Display.print(Result[i],HEX);
  }
  Serial.println(EMPTY_STR);
  if(onDisplay) Display.println(EMPTY_STR);
}

//drain all characters from serial buffer
void drainSerial() {
  while(Serial.available()>0) {
    Serial.read();
  }
}

bool CheckFibonacci(uint8_t prime, uint32_t value) {
  uint8_t c;
  int32_t first = 0, second = 1, next;
 
  for ( c = 0 ; c <=prime ; c++ ) {
    if ( c <= 1 ) {
      next = c;
    } else {
      next = first + second;
      first = second;
      second = next;
    }
    //Serial.println(next);
  }
  if(next==value) {
    return true;
  }
  return false; 
}

//static const short CGLH=16;

//The life function is the most important function in the program.
//It counts the number of cells surrounding the center cell, and 
//determines whether it lives, dies, or stays the same.
void life(unsigned int *array, char choice, short width, short height, unsigned int *temp) {
  //Copies the main array to a temp array so changes can be entered into a grid
  //without effecting the other cells and the calculations being performed on them.
  memcpy(&temp[0],&array[0],sizeof(temp));
  for(int j = 1; j < height-1; j++) {
    for(int i = 1; i < width-1; i++) {	
      if(choice == 'm') {
        //The Moore neighborhood checks all 8 cells surrounding the current cell in the array.
        int count = 0;
        count = ((array[j-1]&(1<<i))>0?1:0) + ((array[j-1]&(1<<(i-1)))>0?1:0) 
          + ((array[j]&(1<<(i-1)))>0?1:0) + ((array[j+1]&(1<<(i-1)))>0?1:0)
          + ((array[j+1]&(1<<i))>0?1:0) + ((array[j+1]&(1<<(i+1)))>0?1:0) 
          + ((array[j]&(1<<(i+1)))>0?1:0) + ((array[j-1]&(1<<(i+1)))>0?1:0);
        if(count < 2 || count > 3)
          temp[j]&=~(1<<i);
        if(count == 3)
          temp[j]|=(1<<i);
      } else if(choice == 'v') {
        //The Von Neumann neighborhood checks only the 4 surrounding cells in the array, (N, S, E, and W).
        int count = 0;
        count = ((array[j-1]&(1<<i))>0?1:0) + ((array[j]&(1<<(i-1)))>0?1:0) 
          + ((array[j+1]&(1<<i))>0?1:0) + ((array[j]&(1<<(i+1)))>0?1:0);	
        //The cell dies.  
        if(count < 2 || count > 3)
          temp[j]&=~(1<<i);
          //The cell either stays alive, or is "born".
        if(count == 3)
          temp[j]|=(1<<i);
      }				
    }
  }
  //Copies the completed temp array back to the main array.
  memcpy(&array[0],&temp[0],sizeof(temp));
}


void loop() {
  static uint8_t count = 0;
  if((count%20)==0) {
    dumpDatabaseToSerial();  // Dump DB to serial
  }
  uint32_t start;

  if (PackedVars.LEDMode == MODE_MORSE_CODE_EPIC) { // Normal Morse code
    Serial.println(F("Begin Mode: 0"));
    //Display.fillCircle(Display.width()/2, Display.height()/2, 20, WHITE);
    //Display.display();
    sendMorseStr(F("HHVMORSE DASH "));
    sendMorseStr(GUID);
    sendMorseStr(F("HTTPS COLON SLASH SLASH DCDARK DOT NET    SEND CODES"));
    uint16_t numMsgs = getNumMsgs();
    if (isNumMsgsValid(numMsgs)) {
      for (int msgAddr = 0; msgAddr < numMsgs*16; msgAddr+=16) {
        dumpDatabaseToSerial();
        sendMorseTwitter(msgAddr);
      }
    }
  } else if (PackedVars.LEDMode == MODE_SNORING) { // Snoring mode
    Serial.println(F("Begin Mode: 1"));
    start = millis();
    rampLED(0, 248, 0, 124, 0, 124, 0, 124, 0, 124, 1600);
    rampLED(248, 0, 124, 0, 124, 0, 124, 0, 124, 0, 1600);
    beaconGUID();
    delayAndReadIR(5000-(millis()-start));
  } else if (PackedVars.LEDMode == MODE_BACKLIGHT) {  
    start = millis();
    // Lub
    rampLED(0, 200, 0, 0, 0, 0, 0, 0, 0, 0, 60);
    rampLED(200, 0, 0, 0, 0, 0, 0, 0, 0, 0, 120);
    delayAndReadIR(400);

    // Dub
    //      LED  LED  BACK 1    BACK 2    BACK 3    BACK 4
    rampLED(0,   250, 0,   0,   0,   0,   0,   0,   0,   0,   60);
    rampLED(250, 0,   0,   0,   0,   250, 0,   250, 0,   0,   120);
    rampLED(0,   0,   0,   250, 250, 150, 250, 150, 0,   250, 120);
    rampLED(0,   0,   250, 150, 150, 50,  150, 50,  250, 150, 120);
    rampLED(0,   0,   150, 100, 50,  0,   50,  0,   150, 100, 300);
    //todo check to see if this one still cashes crash every once in a while
    rampLED(0,   0,   100, 0,   0,   0,   0,   0,   100, 0,   500);
      
    beaconGUID();
    delayAndReadIR(4000-(millis()-start));
  } else if (PackedVars.LEDMode == MODE_SERIAL_EPIC) {
    unsigned long SerialEpicTimer = millis();
    PackedVars.Silent = 1;
    Serial.println(F("Password: "));
    while((millis()-SerialEpicTimer) < MAX_SERIAL_EPIC_TIME_MS ) { //don't loop for MAX_SERIAL_EPIC_TIME
      start = millis();
      if(0==PackedVars.InSerialEpic) {
        if( Serial.available()>0) { //are we trying to enter the serial epic?
          if(Serial.available()==MIN_SERIAL_LEN) {
            for(short n = 0; n<MIN_SERIAL_LEN;n++) {
              char s = toupper(Serial.read());
              if(s==START_SERIAL_EPIC_STRING[n]) {
                PackedVars.InSerialEpic = 1;  
              }
            }
          } else {
            Serial.println(iKnowNot);
            PackedVars.InSerialEpic = 0;
          }
        }
      } else {
        if(0==PackedVars.AwaitingSerialAnswer) { //send question
          //Serial.println(F("What is the answer to all things?"));
          PackedVars.AwaitingSerialAnswer = 1;
          GenerateResponseToCorrectEpic(START_SERIAL_EPIC_STRING,false);
        } else {
          #if 0
          if(Serial.available()) {
            PackedVars.AwaitingSerialAnswer = 0;
            if(Serial.available() == strlen(SERIAL_PORT_ANSWER)) {
              char serialAnswer[MAX_SERIAL_ANSWER_LENGTH+1] = {'\0'};
              short i = 0;
              while(Serial.available()) {
                serialAnswer[i++] = Serial.read();
              }
              //Serial.println((char *)&serialAnswer[0]);
              if(strcmp(SERIAL_PORT_ANSWER,&serialAnswer[0])==0) {
                GenerateResponseToCorrectEpic(SERIAL_PORT_ANSWER,false);
                break;
              } else {
                Serial.println(iKnowNot);
              }
            } else {
              Serial.println(iKnowNot);
            }
          }
          #endif
        }
      }
      drainSerial();
      beaconGUID();
      delayAndReadIR(1000-(millis()-start));
    }
    if((millis()-SerialEpicTimer) >= MAX_SERIAL_EPIC_TIME_MS) {
      Serial.println(F("Times up."));
    }
    PackedVars.InSerialEpic = 0;
    PackedVars.AwaitingSerialAnswer = 0;
  } else if(PackedVars.LEDMode == MODE_DISPLAY_BOARD_EPIC) {
    if(!PackedVars.DisplayAnswer) {
      start = millis();
      static char Answer[10] = {'\0'};
      static uint8_t count = 0;
      Display.setTextWrap(true);
      //Serial.println(digitalRead(BUTTON_UP)==LOW);
      //Serial.println(analogRead(BUTTON_LEFT));
      //Serial.println(analogRead(BUTTON_RIGHT));
      //Serial.println(analogRead(BUTTON_DOWN));
      //Serial.println(analogRead(BUTTON_CENTER));
      uint16_t b1 = ( (digitalRead(BUTTON_UP)==LOW?:0) | 
          (analogRead(BUTTON_LEFT)<10?VLEFT:0) | 
          (analogRead(BUTTON_RIGHT)<10?VRIGHT:0) | 
          (analogRead(BUTTON_DOWN)<10?VDOWN:0) | 
          (analogRead(BUTTON_CENTER)<10?VCENTER:0));
      delay(4);
      
      uint16_t button = b1 & ( (digitalRead(BUTTON_UP)==LOW?VUP:0) | 
        (analogRead(BUTTON_LEFT)<10?VLEFT:0) | (analogRead(BUTTON_RIGHT)<10?VRIGHT:0) | 
        (analogRead(BUTTON_DOWN)<10?VDOWN:0) | (analogRead(BUTTON_CENTER)<10?VCENTER:0));
      
      if(button&VLEFT) {
        PackedVars.LINE1Loc = PackedVars.LINE1Loc==0?LINE1_LENGTH-1:PackedVars.LINE1Loc-1;
      } 
      if(button&VRIGHT) {
        PackedVars.LINE1Loc = ++PackedVars.LINE1Loc%LINE1_LENGTH;
      }
      if(button&VUP) {
        short tmp = PackedVars.LINE1Loc-LINE1_LENGTH;
        if(tmp<0) {
         PackedVars.LINE1Loc=LINE1_LENGTH+PackedVars.LINE1Loc; 
        } else {
         PackedVars.LINE1Loc=tmp; 
        }
      }
      if(button&VDOWN) {
        PackedVars.LINE1Loc = (PackedVars.LINE1Loc+LINE1_LENGTH/2)%LINE1_LENGTH;
      }
      
      if(button==(VRIGHT|VLEFT)) {
         memset(&Answer[0],0,sizeof(Answer)); 
         PackedVars.AnswerPos = 0;
         PackedVars.LINE1Loc = 0;
      }
      
      if(button&VCENTER) {
        Answer[PackedVars.AnswerPos] = LINE1[PackedVars.LINE1Loc];
        PackedVars.AnswerPos = (PackedVars.AnswerPos+1)>=sizeof(Answer) ? sizeof(Answer) : PackedVars.AnswerPos+1;
      }
      
      if(strcmp(&Answer[0],&DisplayEpicAnswer[0])==0) {
        Display.clearDisplay();
        Display.setCursor(0,0);
        PackedVars.DisplayAnswer = 1;
        GenerateResponseToCorrectEpic(&DisplayEpicAnswer[0],true);
        //GenerateResponseToCorrectEpic(Questions[STANDARD_SERIAL_EPIC].Answer,true);
        Display.display();
      } else {
        //Serial.println(&Answer[0]);
        uint8_t location = 0;
        Display.setCursor(0,0);
        Display.println(DisplayEpicText);
        Display.println(EMPTY_STR);
        Display.println(EMPTY_STR);
        Display.print(&Answer[0]);
        if((++count)&0x1) {
          Display.println("_");
        } else {
          Display.println(LINE1[PackedVars.LINE1Loc]);
        }
        Display.println(EMPTY_STR);
        Display.println(LINE1);
        Display.display();
        delay(20);
      }
    }
    //8/3 DAC turning off the ability to sync IR with another bade in this mode
    //beaconGUID();
    delayAndReadIR(400);
    
  } else if (PackedVars.LEDMode == MODE_SILK_SCREEN) { 
    PackedVars.Silent = 1;
    if(0==count) {
      Serial.println(F("Enter the sequence: "));
    }
    //fibonacci numbers
    char silkBuffer[6] = {0};
    if(Serial.available()>0) {
      for(int i=0;i<sizeof(silkBuffer)-1;i++) {
        if(Serial.available()) {
          silkBuffer[i] = Serial.read();
        }
      }
      //Serial.println(&silkBuffer[0]);
      uint32_t num = atoi(&silkBuffer[0]);

      if(!PackedVars.hasSilk1) {
        if(CheckFibonacci(13,num)) PackedVars.hasSilk1 = 1;
        else PackedVars.hasSilk1 = PackedVars.hasSilk2 = PackedVars.hasSilk3 = 0;
      } else if(!PackedVars.hasSilk2) {
        if(CheckFibonacci(17,num)) PackedVars.hasSilk2 = 1;
        else PackedVars.hasSilk1 = PackedVars.hasSilk2 = PackedVars.hasSilk3 = 0;
      } else if(!PackedVars.hasSilk3) {
        if(CheckFibonacci(23,num)) PackedVars.hasSilk3 = 1;
        else PackedVars.hasSilk1 = PackedVars.hasSilk2 = PackedVars.hasSilk3 = 0;
      }
      
      if(PackedVars.hasSilk1 && PackedVars.hasSilk2 && PackedVars.hasSilk3) {
        GenerateResponseToCorrectEpic(&LINE1[10],false); //start with K for Krux!
      } else {
        Serial.print(F("Received: "));
        Serial.println((const char *)&silkBuffer[0]);
      }
    }
    
    drainSerial();
    beaconGUID();
    delayAndReadIR(2000);
  } else if (PackedVars.LEDMode == MODE_JUST_A_COOL_DISPLAY) {
    start = millis();
    static const int width = 32;
    static const int height = 32;
    char neighborhood = (start&1)==0?'m':'v';
    short chanceToBeAlive = rand()%25;
    unsigned int gol[height] = {0};
    unsigned int tmp[height];
    for(int j = 1; j < height-1; j++) {
      for (int i = 1; i < width-1; i++) {
        if((rand()%chanceToBeAlive)==0) {
          //gol[j] &= ~(1<<i);
          gol[j] |= (1<<i);
        } else  {
          //gol[j] |= (1<<i);
        }
      }
    }
    short generations = 100+(rand()%25);
    Display.clearDisplay();
    Display.setCursor(0,20);
    Display.print(F("Max Generations: "));
    //Display.print(F("Generations: ") );
    Display.println(generations);
    Display.display();
    delayAndReadIR(2000);
    for(int i=0;i<generations;i++) {
      int count = 0;
      Display.clearDisplay();	
      for(int j = 1; j < height-1; j++) {
        for(int k = 1; k < width-1; k++) {	
          if((gol[j]&(k<<i)) !=0) {
            Display.drawPixel(k*4,j*2,WHITE);
            count++;
          }
 	      }
      }
      if(0==count) {
        Display.setCursor(20,20);
        Display.println(F("ALL DEAD"));
        Display.print(F("After "));
        Display.print(generations);
        Display.println(F(" generations."));
        Display.display();
        break; 
      } else {
        Display.display();
        life(&gol[0], neighborhood, width,height, &tmp[0]);
        delayAndReadIR(200);
      }
    }
    delayAndReadIR(2000);
  } else if(PackedVars.LEDMode == MODE_UBER_BADGE_SYNC) {
    PackedVars.Silent = 1;
    Display.clearDisplay();
    Display.setCursor(0, 0);
    if (PackedVars.isContestRunner) {
      Display.println(F("Press center button to send: "));
      Display.display();
      start = millis();
      while ((millis() - start) < 2000) {
        if (analogRead(BUTTON_CENTER) < 5) {
          delay(4);
          //Serial.println(analogRead(BUTTON_CENTER));
          if (analogRead(BUTTON_CENTER) < 5) {
            Display.println(F("SENDING..."));
            Display.display();
            PackedVars.swapKeyAndGUID = 1;
            beaconGUID();
            delayAndReadIR(3000);
            Display.println(F("SENT!"));
            Display.display();
            break;
          }
        }
      }
    } else {
      Display.println(F("Ready to receive:"));
      Display.display();
      delayAndReadIR(3000);
    }
  } else { //if (PackedVars.LEDMode == MODE_SHUTDOWN) { // We should never get here.  This is a sign we didn't sleep right.
    sendMorse('C');
  }
  count = (count+1)%100;
}

//////////////////////////////////////////
// code to calibrate oscillator
static void calibrateOscillator(void)
{
  SERIAL_INFO("Before calibrate: ");SERIAL_INFO_LN(OSCCAL);
  uchar step = 128;
  uchar trialValue = 0, optimumValue;
  int   x, optimumDev, targetValue = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);
  //Serial.print("Target: " );Serial.println(targetValue);
 
    /* do a binary search: */
    do{
        OSCCAL = trialValue + step;
        x = usbMeasureFrameLength();    // proportional to current real frequency
        //Serial.print("X: "); Serial.println(x);
        //Serial.print("S: ");Serial.println(step);
        //Serial.print("TV: ");Serial.println(trialValue);
        //Serial.print("OS: ");Serial.println(OSCCAL);
        if(x < targetValue)             // frequency still too low
            trialValue += step;
        step >>= 1;
    }while(step > 0);
    /* We have a precision of +/- 1 for optimum OSCCAL here */
    /* now do a neighborhood search for optimum value */
    optimumValue = trialValue;
    //Serial.println(trialValue);
    optimumDev = x; // this is certainly far away from optimum
    for(OSCCAL = trialValue - 1; OSCCAL <= trialValue + 1; OSCCAL++){
      //Serial.println(OSCCAL);
        x = usbMeasureFrameLength() - targetValue;
        if(x < 0)
            x = -x;
        if(x < optimumDev){
            optimumDev = x;
            optimumValue = OSCCAL;
        }
    }
    OSCCAL = optimumValue;
    SERIAL_INFO("After Calibrate: ");SERIAL_INFO_LN(OSCCAL);
}

void usbEventResetReady(void)
{
  //Serial.println("usbEventReset");
  cli();  // usbMeasureFrameLength() counts CPU cycles, so disable interrupts.
  calibrateOscillator();
  sei();
   //eeprom_write_byte(0, OSCCAL);   // store the calibrated value in EEPROM
}


