#include <avr/pgmspace.h>
#include "IRSerial-2014.h"
#include "MD5.h"
#include <EEPROM.h>
#include <avr/sleep.h>
//#include <Wire.h>
//#include <SPI.h>
//#include <Adafruit_GFX.h>
//#include <Adafruit_SSD1306.h>

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

// Backlight LEDs
#define BACKLIGHT_4 5
#define BACKLIGHT_3 6
#define BACKLIGHT_2 10
#define BACKLIGHT_1 11

//BEGIN SERIAL EPIC VARS
//string that starts serial epic
#define START_SERIAL_EPIC_STRING "CMDC0DE"
//length of serial string
#define MIN_SERIAL_LEN  7
//bool to see if we have started the SerialEpic
#define MAX_SERIAL_EPIC_TIME_MS 30000
#define STANDARD_SERIAL_EPIC 0
#define MAX_SERIAL_ANSWER_LENGTH 10
#define NUM_QUESTIONS 1

struct _Questions {
  const char * Text;
  const char * Answer;
};

_Questions Questions[NUM_QUESTIONS]={
    { "What is the answer to all things?","42" }
};


//END SERIAL EPIC VARS

//BEGIN GLOBAL VARS:
//We are getting tight on space so I'm packing variables as tightly as I can
struct _PackedVars {
  unsigned short InSerialEpic : 1;
  unsigned short AwaitingSerialAnswer: 1;
  unsigned short LEDMode : 3; //what start up mode are we need (animation)
  unsigned short Silent : 1;
} PackedVars;

uint16_t KEY[4];
char GUID[9];
unsigned long nextBeacon;
#define OLED_RESET 4
//Adafruit_SSD1306 Display(OLED_RESET);
//END GLOBAL VARS


//BEGIN MODE_DEFS
#define MODE_COUNT 6
#define MODE_MORSE_CODE_EPIC     0
#define MODE_SNORING             1
#define MODE_HEARTBEAT           2
#define MODE_SERIAL_EPIC         3
#define MODE_DISPLAY_BOARD_EPIC  4
#define MODE_SHUTDOWN            5
//END MODE DEFS


// BEGIN EEPROM count location
#define MSG_COUNT_ADDR 1022
#define RESET_STATE_ADDR 1020
#define GUID_ADDR 1012
#define KEY_ADDR 1004
#define PERSISTENT_EPIC_FLAG_ADDR 1000

// Maximum number of messages
#define MAX_NUM_MSGS 60
#define GUID_SIZE 8
#define MORSE_CODE_ENCODED_MSG 8
#define TOTAL_STORAGE_SIZE_MSG (GUID_SIZE+MORSE_CODE_ENCODED_MSG)
#define MAX_MSG_ADDR = (TOTAL_STORAGE_SIZE_MSG*MAX_NUM_MSGS)


// END EEPROM COUNT LOCATION

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
#define SERIAL_TRACE 
#ifdef SERIAL_TRACE
  #define SERIAL_TRACE_LN(a) Serial.println(a);
  #define SERIAL_TRACE(a) Serial.println(a);
#else
  #define SERIAL_TRACE_LN(a)
  #define SERIAL_TRACE(a)
#endif

#define SERIAL_INFO 
#ifdef SERIAL_INFO
  #define SERIAL_INFO_LN(a) Serial.println(a);
  #define SERIAL_INFO(a) Serial.print(a);
#else
  #define SERIAL_INFO_LN(a)
  #define SERIAL_INFO(a)
#endif
//end debugging macros
 

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
    if(PackedVars.Silent) {
      SERIAL_INFO_LN(F("Ping!"));
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

void sendMorseStr(char *msg) {
  for (; *msg != '\0'; msg++) {
    sendMorse(*msg);
    beaconGUID();
  }
}

// LULZ Twitter. That's old school...  Haven't used Twitter for this since 2012.
void sendMorseTwitter(uint16_t msgAddr) {
  sendMorseStr("DKN DASH ");
  for (unsigned char ndx = 0; ndx < 8; ndx++) {
    sendMorse(EEPROM.read(msgAddr+ndx));
  }
}

void sendSerialTwitter(uint16_t msgAddr) {
  Serial.print(F("DKN-"));
  for (unsigned char ndx = 0; ndx < 16; ndx++) {
    Serial.write(EEPROM.read(msgAddr+ndx));
  }
  Serial.println("");
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
    Serial.print(F("numMsgs not valid. Setting to 0. This is normal if this is your first pairing."));
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
      Serial.print("readWordFromBuf() line noise: ");
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
    Serial.print("Bad rx header: ");
    Serial.println(c);
    return false;
  }
  for (int i = -10; i < -2; i++) {
    c = rxBufNdx(i);
    
    // If it's not a letter and not a number
    if (!(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9')) {  
      Serial.print("Bad rx data: ");
      Serial.print(i);
      Serial.print(" ");
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
  return writeEEPROM(bobGUID, bobEnc);
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
  }
  else if (flag == 'y') {
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
  Serial.print(F("Number of messages: "));
  Serial.println(numMsgs);
  Serial.println(F("https://dcdark.net/  Send the following codes:"));
  Serial.print(F("HHVSERIAL-"));
  Serial.println(GUID);
  
  if (isNumMsgsValid(numMsgs)) {
    for (int msgAddr = 0; msgAddr < numMsgs*16; msgAddr+=16)
      sendSerialTwitter(msgAddr);
    Serial.println("");
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

  UsbKeyboard = new UsbKeyboardDevice();
  
  usbDelay(5000);
  // Give the USB a few seconds to settle in and be detected by the OS.
  //if (waitForHost(10000)) {
  if(usbInterruptIsReady()) {
    SERIAL_INFO_LN(F("USB is ready."));
    usbDelay(100);
    SERIAL_TRACE_LN("back from delay");
    printUSB("https://dcdark.net/  Send the following codes:\n");
    printUSB("HHVUSB-");
    printUSB(GUID);
    printUSB("\n");
    //Serial.println("back from printing");
    uint16_t numMsgs = getNumMsgs();
    if (isNumMsgsValid(numMsgs)) {
      for (int msgAddr = 0; msgAddr < numMsgs*16; msgAddr+=16) {
        Serial.println(".");
        //DAC - adding because if there are enough messages the usb will lock up because update has not been called.
        UsbKeyboard->update();
        sendUSBTwitter(msgAddr);
      }
      printUSB("\n");
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
  printUSB("DKN-");
  uint8_t p;
  for (unsigned char ndx = 0; ndx < 16; ndx++) {
    p = EEPROM.read(msgAddr+ndx);
    writeUSB(p);
  }
  printUSB("\n");
}

// copied from Print.cpp
void printUSB(const __FlashStringHelper *str) {
  printUSB(reinterpret_cast<const char*>(str));
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
    Serial.print("ERROR: Unknown character: ");
    Serial.print(int(c));
    Serial.print(" ");
    Serial.write(c);
    Serial.println("");
  }
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
  
  delay(200);  // Reset "debounce"

  // Setup the IR buffers and timers.
  nextBeacon = millis();  
  clearRxBuf();

  // Pull GUID and Private key from EEPROM
  for (uchar ndx=0; ndx < 8; ndx++) {
    GUID[ndx] = EEPROM.read(GUID_ADDR + ndx);
  }
  GUID[8] = 0;
  // Yes, this is big endian. I don't want to have to byte-swap
  // when building the EEPROM file from text strings.
  KEY[0] = (EEPROM.read(KEY_ADDR+0)<<8) + EEPROM.read(KEY_ADDR+1);
  KEY[1] = (EEPROM.read(KEY_ADDR+2)<<8) + EEPROM.read(KEY_ADDR+3);  
  KEY[2] = (EEPROM.read(KEY_ADDR+4)<<8) + EEPROM.read(KEY_ADDR+5);  
  KEY[3] = (EEPROM.read(KEY_ADDR+6)<<8) + EEPROM.read(KEY_ADDR+7);  
  
  //set initial Pack Vars
  PackedVars.InSerialEpic = 0; // no serial received yet so we are not on serial epic
  PackedVars.AwaitingSerialAnswer = 0; //no serial epic active yet so we don't have a state for it

  // BLINKY SHINY!  
  PackedVars.LEDMode = EEPROM.read(RESET_STATE_ADDR)%MODE_COUNT;
  EEPROM.write(RESET_STATE_ADDR, (PackedVars.LEDMode + 1)%MODE_COUNT); 
 
  
  #if 0
    PackedVars.LEDMode = MODE_DISPLAY_BOARD_EPIC;
  #endif
  
  if (PackedVars.LEDMode == MODE_MORSE_CODE_EPIC) {        // Normal Morse code
    Serial.println(F("Morse output of codes..."));
    sendMorse('E');          // .
  } else if (PackedVars.LEDMode == MODE_SNORING) { // Snoring
    Serial.println(F("Snoring..."));
    sendMorse('I');          // ..
  } else if (PackedVars.LEDMode == MODE_HEARTBEAT) { // Heartbeat
    Serial.println(F("Heart beat..."));
    sendMorse('S');          // ...
  } else if (PackedVars.LEDMode == MODE_SERIAL_EPIC) { //epic
    Serial.println(F("Operative"));
    sendMorse('T');
  } else if (PackedVars.LEDMode == MODE_DISPLAY_BOARD_EPIC) {
    Serial.println(F("Uber Operative"));
    sendMorse('D');
    //Display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
    //Display.display();
    //delay(1000);
    //Display.fillCircle(Display.width()/2, Display.height()/2, 10, WHITE);
    //Display.display();
  } else { //if (PackedVars.LEDMode == MODE_SHUTDOWN) { // Off
    Serial.println(F("Shutting down..."));
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
  
  int ledDiff = ledEnd-ledStart;
  int back1Diff = back1End-back1Start;
  int back2Diff = back2End-back2Start;
  int back3Diff = back3End-back3Start;
  int back4Diff = back4End-back4Start;
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

void GenerateResponseToCorrectSerialEpic() {
  //hash or encrypt answer with key?
  MD5_CTX ctx;
  MD5 hasher;
  unsigned char Result[16];
  hasher.MD5Init(&ctx);
  hasher.MD5Update(&ctx,&KEY[0],sizeof(KEY));
  hasher.MD5Update(&ctx,&GUID[0],sizeof(GUID));
  hasher.MD5Final(&Result[0],&ctx);
  Serial.println(F("The daemon will accept the following: " ));
  for(int i=0;i<sizeof(Result);i++) {
    Serial.print(Result[i],HEX); 
  }
  Serial.println("");
}

//drain all characters from serial buffer
void drainSerial() {
  while(Serial.available()>0) {
    Serial.read();
  }
}

void loop() {
  dumpDatabaseToSerial();  // Dump DB to serial
  uint32_t start;
 
  if (PackedVars.LEDMode == MODE_MORSE_CODE_EPIC) { // Normal Morse code
    Serial.println(F("Begin Mode: 0"));
    sendMorseStr("HHVMORSE DASH ");
    sendMorseStr(GUID);
    sendMorseStr("HTTPS COLON SLASH SLASH DCDARK DOT NET    SEND CODES");
    uint16_t numMsgs = getNumMsgs();
    if (isNumMsgsValid(numMsgs)) {
      for (int msgAddr = 0; msgAddr < numMsgs*16; msgAddr+=16) {
        dumpDatabaseToSerial();
        sendMorseTwitter(msgAddr);
      }
    }
  } else if (PackedVars.LEDMode == MODE_SNORING) { // Snoring mode
    // Only dumpDatabaseToSerial() every 10th snore
    Serial.println("Begin Mode: 1");
    for (unsigned char ndx = 0; ndx < 10; ndx++) {
      start = millis();
      rampLED(0, 248, 0, 124, 0, 124, 0, 124, 0, 124, 1600);
      rampLED(248, 0, 124, 0, 124, 0, 124, 0, 124, 0, 1600);
      beaconGUID();
      delayAndReadIR(5000-(millis()-start));
    }
  } else if (PackedVars.LEDMode == MODE_HEARTBEAT) { // Heartbeat mode
    // Only dumpDatabaseToSerial() every 20 beats
    Serial.println("Begin Mode: 2");
    for (unsigned char ndx = 0; ndx < 20; ndx++) {
      start = millis();
      // Lub
      rampLED(0, 200, 0, 0, 0, 0, 0, 0, 0, 0, 60);
      rampLED(200, 0, 0, 0, 0, 0, 0, 0, 0, 0, 120);
      delayAndReadIR(200);
      
      // Dub
      //      LED  LED  BACK 1    BACK 2    BACK 3    BACK 4
      rampLED(0,   250, 0,   0,   0,   0,   0,   0,   0,   0,   60);
      rampLED(250, 0,   0,   0,   0,   250, 0,   250, 0,   0,   120);
      rampLED(0,   0,   0,   250, 250, 150, 250, 150, 0,   250, 120);
      rampLED(0,   0,   250, 150, 150, 50,  150, 50,  250, 150, 120);
      rampLED(0,   0,   150, 100, 50,  0,   50,  0,   150, 100, 300);
      Serial.println("ramp 8");
      rampLED(0,   0,   100, 0,   0,   0,   0,   0,   100, 0,   500);
      Serial.println("ramp 9");
      
      beaconGUID();
      delayAndReadIR(2000-(millis()-start));
    }
  } else if (PackedVars.LEDMode == MODE_SERIAL_EPIC) {
    unsigned long SerialEpicTimer = millis();
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
            Serial.println(F("I know not what you speak of."));
            PackedVars.InSerialEpic = 0;
          }
        }
      } else {
        if(0==PackedVars.AwaitingSerialAnswer) { //send question
          Serial.println(Questions[STANDARD_SERIAL_EPIC].Text);
          PackedVars.AwaitingSerialAnswer = 1;
        } else {
          if(Serial.available()) {
            PackedVars.AwaitingSerialAnswer = 0;
            if(Serial.available() == strlen(Questions[STANDARD_SERIAL_EPIC].Answer)) {
              char serialAnswer[MAX_SERIAL_ANSWER_LENGTH+1] = {'\0'};
              short i = 0;
              while(Serial.available()) {
                serialAnswer[i++] = Serial.read();
              }
              //Serial.println((char *)&serialAnswer[0]);
              if(strcmp(Questions[STANDARD_SERIAL_EPIC].Answer,&serialAnswer[0])==0) {
                GenerateResponseToCorrectSerialEpic();
                break;
              } else {
                Serial.println(F("I know not what you speak of."));
              }
            } else {
              Serial.println(F("I know not what you speak of."));
            }
          }
        }
      }
      drainSerial();
      beaconGUID();
      delayAndReadIR(2000-(millis()-start));
    }
    if((millis()-SerialEpicTimer) >= MAX_SERIAL_EPIC_TIME_MS) {
      Serial.println(F("Times up."));
    }
    PackedVars.InSerialEpic = 0;
    PackedVars.AwaitingSerialAnswer = 0;
  } else if(PackedVars.LEDMode == MODE_DISPLAY_BOARD_EPIC) {
    
    
  } else { //if (PackedVars.LEDMode == MODE_SHUTDOWN) { // We should never get here.  This is a sign we didn't sleep right.
    sendMorse('C');
  }
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


