#include "UsbKeyboard.h"
//UsbKeyboardDevice UsbKeyboard = UsbKeyboardDevice();

#define BUTTON_PIN 12
#define LED_PIN 3

void setup() {
  pinMode(BUTTON_PIN, INPUT);
  digitalWrite(BUTTON_PIN, HIGH);
  pinMode(LED_PIN, OUTPUT);
  usbDeviceDisconnect();
  
}

void usbDelay(long ms, UsbKeyboardDevice *ukd) {
  for (; ms > 25; ms-=25) {
    //digitalWrite(LED_PIN, (ms/250)&0x01);
    analogWrite(LED_PIN, ms%0xFF);
    delay(25);
    ukd->update();
  }
  delay(ms);
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    digitalWrite(LED_PIN, HIGH);
    UsbKeyboardDevice UsbKeyboard = UsbKeyboardDevice();
  
    // Give the USB a few seconds to settle in and be detected by the OS.  
    usbDelay(5000, &UsbKeyboard);
    if (usbInterruptIsReady()) {
      UsbKeyboard.sendKeyStroke(KEY_H);
      UsbKeyboard.sendKeyStroke(KEY_E);
      UsbKeyboard.sendKeyStroke(KEY_L);
      UsbKeyboard.sendKeyStroke(KEY_L);
      UsbKeyboard.sendKeyStroke(KEY_O);

      UsbKeyboard.sendKeyStroke(KEY_SPACE);

      UsbKeyboard.sendKeyStroke(KEY_W);
      UsbKeyboard.sendKeyStroke(KEY_O);
      UsbKeyboard.sendKeyStroke(KEY_R);
      UsbKeyboard.sendKeyStroke(KEY_L);
      UsbKeyboard.sendKeyStroke(KEY_D);

      UsbKeyboard.sendKeyStroke(KEY_ENTER);
      // Give the OS a second to accept the last of the characters and really be done.
      usbDelay(1000, &UsbKeyboard);
    }
    
    usbDeviceDisconnect();
    digitalWrite(LED_PIN, LOW);
  }
}

void usbEventResetReady(void) {
  
}
