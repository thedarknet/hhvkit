
#define SERIAL_BAUD 115200
#include <Wire.h>

void setup() {
  // put your setup code here, to run once:
  Serial.begin(SERIAL_BAUD);
  pinMode(10,INPUT);
  digitalWrite(10,HIGH);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.print("analog:  A3: " );
  Serial.println(analogRead(A3));
  Serial.print("analog:  A2: " );
  Serial.println(analogRead(A2));
  Serial.print("analog:  A1: " );
  Serial.println(analogRead(A1));
  Serial.print("analog:  A0: " );
  Serial.println(analogRead(A0));
  Serial.print("digital  ");
  Serial.println(digitalRead(10)==LOW);
  delay(1000);
}
