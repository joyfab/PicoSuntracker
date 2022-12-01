/*(c) Mechanical Moon LLC, All rights reserved.
  Two axis Azimut/Elevation Rx/Tx Suntracker.(Raspberry Pi Pico).
  RP2040-Zero Joy jan 2022. step/dir and RX/TX modes.*/

#include <Arduino.h>
#include <Wire.h>

#include "at24c256.h"
at24c256 eeprom(0x50);

#define STEPPER1_STEP_PIN 26  // X Pulse
#define STEPPER1_DIR_PIN  15  // X Direction
#define STEPPER2_STEP_PIN 29  // Y Pulse 
#define STEPPER2_DIR_PIN  28  // Y Direction

const int ENABLE1 = 6;       // X Enable
const int ENABLE2 = 5;       // Y Enable
const int SW1 = 14;            // X Limit switch
const int SW2 = 27;            // Y Limit switch

#include <AccelStepper.h>
AccelStepper stepper1(AccelStepper::DRIVER, STEPPER1_STEP_PIN, STEPPER1_DIR_PIN);
AccelStepper stepper2(AccelStepper::DRIVER, STEPPER2_STEP_PIN, STEPPER2_DIR_PIN);

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel pixels(1, 16, NEO_GRB + NEO_KHZ800);

#include <Helios.h>
Helios helios;                // sun/earth positioning calculator
double dAzimuth;              // Azimut angle
double dElevation;            // Elevation angle

#include <DS3231.h>
DS3231 Clock;                 // RTC
bool Century = false;
bool h12;
bool PM;
byte ADay, AHour, AMinute, ASecond, ABits;
bool ADy, A12h, Apm;
byte year, month, date, DoW, hour, minute, second;

unsigned int AzOffset;        // Azimuth Offset
unsigned int ElOffset;        // Elevation Offset
unsigned int AzBack;          // Azimuth Backoff SW1
unsigned int ElBack;          // Elevation Backoff SW2
unsigned int Angl;            // Start/End elevation angle
unsigned int xReach;
unsigned int yReach;
unsigned int lastx = 0;
unsigned int lasty = 0;

int ResponseDelay;            // Time delay (microsecond)
int xcompt = 0;
int ycompt = 0;
int Status = 0;               // Day/Night

char blop;
String input = "";

void setup() {

  /*    Clock.setSecond(0);     //Set the second
        Clock.setMinute(36);    //Set the minute
        Clock.setHour(20);      //Set the hour
        Clock.setDoW(7);        //Set the day of the week
        Clock.setDate(8);       //Set the date of the month
        Clock.setMonth(10);     //Set the month of the year
        Clock.setYear(22);      //Set the year (Last two digits of the year)*/

  Serial.begin(9600);   // Usb
  Serial2.begin(9600);  // Ble

  pinMode(STEPPER1_STEP_PIN, OUTPUT);
  pinMode(STEPPER1_DIR_PIN, OUTPUT);
  pinMode(STEPPER2_STEP_PIN, OUTPUT);
  pinMode(STEPPER2_DIR_PIN, OUTPUT);
  pinMode(ENABLE1, OUTPUT);
  pinMode(ENABLE2, OUTPUT);
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);

  Wire.setSDA(12);
  Wire.setSCL(13);
  Wire.setClock(100000);
  Wire.begin();

  eeprom.init();
  AzOffset = eeprom.read(30000);
  ElOffset = eeprom.read(30001);
  AzBack = eeprom.read(30002);
  ElBack = eeprom.read(30003);
  Angl = eeprom.read(30004);

  pixels.begin();
  pixels.clear();
  for (int i = 0; i < 1; i++) {
    pixels.setPixelColor(i, pixels.Color(1, 0, 0));
    pixels.show();
  }
  digitalWrite(ENABLE1, HIGH);
  digitalWrite(ENABLE2, HIGH);
  delay(2000);
  XHoming();                         // Homing back off.
  delay(1000);
  YHoming();                         // Homing back off.
  delay(1000);
  stepper1.setMaxSpeed(100000.0);
  stepper1.setSpeed(100000.0);
  stepper1.setAcceleration(1000.0);
  stepper2.setMaxSpeed(100000.0);
  stepper2.setSpeed(100000.0);
  stepper2.setAcceleration(1000.0);
  delay(1000);
  stepper1.moveTo(-AzBack * 100);
  stepper1.runToPosition();
  XHoming();
  stepper1.moveTo(0);
  stepper1.runToPosition();
  delay(1000);
  stepper2.moveTo(-ElBack * 100);
  stepper2.runToPosition();
  YHoming();
  stepper2.moveTo(0);
  stepper2.runToPosition();
}
void ReadDS3231()  {
  int second, minute, hour, date, month, year, temperature;
  second = Clock.getSecond();
  minute = Clock.getMinute();
  hour = Clock.getHour(h12, PM);
  date = Clock.getDate();
  month = Clock.getMonth(Century);
  year = Clock.getYear();
  temperature = Clock.getTemperature();
  delay(10);
}
void XHoming() {
  digitalWrite(STEPPER1_DIR_PIN, LOW);
  ResponseDelay = 100;
  xcompt = 0;
  for (xcompt = 0 ; xcompt < 200000 ; xcompt++) {
    digitalWrite(STEPPER1_STEP_PIN, HIGH);
    delayMicroseconds(1);
    digitalWrite(STEPPER1_STEP_PIN, LOW);
    delayMicroseconds(ResponseDelay);
    if (digitalRead(SW1) == LOW)  {
      xcompt = 200000;
    }
  }
}
void YHoming() {
  digitalWrite(STEPPER1_DIR_PIN, LOW);
  ResponseDelay = 100;
  ycompt = 0;
  for (ycompt = 0 ; ycompt < 200000 ; ycompt++) {
    digitalWrite(STEPPER2_STEP_PIN, HIGH);
    delayMicroseconds(1);
    digitalWrite(STEPPER2_STEP_PIN, LOW);
    delayMicroseconds(ResponseDelay);
    if (digitalRead(SW2) == LOW)  {
      ycompt = 200000;
    }
  }
}
void loop() {    //longitude and latitude example: 5°37'43.66"E,44°18'58.91"N  => 5.62879,44.31636 decimal.
  ReadDS3231();  // see https://www.coordonnees-gps.fr/conversion-coordonnees-gps
  helios.calcSunPos(Clock.getYear(), Clock.getMonth(Century), Clock.getDate(), Clock.getHour(h12, PM), Clock.getMinute(), Clock.getSecond(), 5.62879, 44.31636); // write here the decimal.
  dAzimuth = helios.dAzimuth;
  dElevation = helios.dElevation;
  float ElReach = ((dElevation * 16576 / 3) + (ElOffset * 1000) - 116032);
  float AzReach = ((dAzimuth * 16576 / 3) + (AzOffset * 1000) - 497280);
  delay(100);                            // Conditions
  if (dAzimuth > 267) {
    dAzimuth == 267;                     // limit west
  }
  if (dAzimuth < 93) {
    dAzimuth == 93;                      // limit east
  }
  if (dElevation > (Angl - 0.2))         // Sunset Power On Motors
    if (dElevation < (Angl - 0.1))  {
      digitalWrite(ENABLE1, HIGH);
      digitalWrite(ENABLE2, HIGH);
      delay(1000);
    }
  if (dElevation > (Angl - 0.6))
    if (dElevation < (Angl - 0.5))  {    // Sunrise Power Off motors to save power
      digitalWrite(ENABLE1, LOW);
      digitalWrite(ENABLE2, LOW);
      delay(1000);
    }
  if (dElevation > Angl)  {              // jour
    Status = 1;
    pixels.clear();
    for (int i = 0; i < 1; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 1, 0));
      pixels.show();
    }
    delay(1000);
  }
  if (dElevation < Angl)  {              // nuit
    Status = 0;
    pixels.clear();
    for (int i = 0; i < 1; i++) {
      pixels.setPixelColor(i, pixels.Color(1, 0, 0));
      pixels.show();
    }
    delay(1000);
  }
  if (Status == 0) {                     // night wind position
    AzReach = 909180;                    // 255°
    ElReach = 364672;                    // 66 + 21 = 87°
  }
  xReach = AzReach - lastx;
  yReach = ElReach - lasty;
  if ((Clock.getSecond() == 0) || (Clock.getSecond() == 5) || (Clock.getSecond() == 10) || (Clock.getSecond() == 15) || (Clock.getSecond() == 20) || (Clock.getSecond() == 25) || (Clock.getSecond() == 30) || (Clock.getSecond() == 35) || (Clock.getSecond() == 40) || (Clock.getSecond() == 45) || (Clock.getSecond() == 50) || (Clock.getSecond() == 55)) {
    delay(100);
    stepper1.moveTo(xReach);
    stepper1.runToPosition();
    delay(100);
    lastx == AzReach;
    stepper2.moveTo(yReach);
    stepper2.runToPosition();
    delay(100);
    lasty == ElReach;
    delay(100);
  }
  if (Serial2.available())                   //   BLE control
    while (Serial2.available() > 0) {
      char inChar = (char)Serial2.read();
      input += inChar;
      pixels.clear();
      for (int i = 0; i < 1; i++) {
        pixels.setPixelColor(i, pixels.Color(0, 0, 250));
        pixels.show();
      }
    }
  if (input.length() >= 1) {
    {
      blop = Serial2.read();
    }
    if (input == "Wind") {                   // Wind Position and lock
      AzReach = 909180;                      // 255°
      ElReach = 364672;                      // 66 + 21 = 87°
      Serial2.println(" ");
      Serial2.println("Wind Position");
      stepper1.moveTo(AzReach);
      stepper1.runToPosition();
      delay(1000);
      lastx == AzReach;
      stepper2.moveTo(ElReach);
      stepper2.runToPosition();
      delay(1000);
      lasty == ElReach;
      digitalWrite(ENABLE1, LOW);
      digitalWrite(ENABLE2, LOW);
      delay(1000);
    }
    if (input == "Star") {              // Unlock and Start
      AzReach = 909180;  // 255°
      ElReach = 364672;  // 66 + 21 = 87°
      digitalWrite(ENABLE1, HIGH);
      digitalWrite(ENABLE2, HIGH);
      delay(1000);
    }
    if (input == "Home") {              // Homing
      Serial2.println(" ");
      Serial2.println("Homing...");
      stepper1.moveTo(0);
      stepper1.runToPosition();
      XHoming();
      delay(1000);
      stepper1.moveTo(AzBack * 100);
      stepper1.runToPosition();
      delay(1000);
      stepper2.moveTo(0);
      stepper2.runToPosition();
      YHoming();
      delay(1000);
      stepper2.moveTo(ElBack * 100);
      stepper2.runToPosition();
      delay(1000);
    }
    if (input == "Test") {             // Test commands
      Serial2.println(" ");
      Serial2.println("Test OK");
      delay(2000);
    }
    if (input == "Rsto") {            // Reset Offset to 10
      Serial2.println("");
      Serial2.print("AzOffset = ");
      AzOffset = 100;
      eeprom.write(30000, 100);
      Serial2.print(AzOffset, DEC);
      Serial2.println("");
      Serial2.print("ElOffset = ");
      ElOffset = 100;
      eeprom.write(30001, 100);
      Serial2.print(ElOffset, DEC);
      Serial2.println("");
      Serial2.print("AzBack = ");
      AzBack = 100;
      eeprom.write(30002, 100);
      Serial2.print(AzBack, DEC);
      Serial2.println("");
      Serial2.print("ElBack = ");
      ElBack = 100;
      eeprom.write(30003, 100);
      Serial2.print(ElBack, DEC);
      Serial2.println("");
      Serial2.print("StartAngle = ");
      Angl = 12;
      eeprom.write(30004, 12);
      Serial2.println(Angl, DEC);
    }
    if (input == "Az+") {                   // Azimuth Offset +
      Serial2.println("");
      Serial2.print("AzOffset = ");
      AzOffset = AzOffset + 1;
      Serial2.println(AzOffset, DEC);
      eeprom.write(30000, AzOffset);
      delay(200);
    }
    if (input == "Az-") {                   // Azimuth Offset -
      Serial2.println("");
      Serial2.print("AzOffset = ");
      AzOffset = AzOffset - 1;
      Serial2.println(AzOffset, DEC);
      eeprom.write(30000, AzOffset);
      delay(200);
    }
    if (input == "El+") {                   // Elevation Offset +
      Serial2.println("");
      Serial2.print("ElOffset = ");
      ElOffset = ElOffset + 1;
      Serial2.println(ElOffset, DEC);
      eeprom.write(30001, ElOffset);
      delay(200);
    }
    if (input == "El-") {                   // Elevation Offset -
      Serial2.println("");
      Serial2.print("ElOffset = ");
      ElOffset = ElOffset - 1;
      Serial2.println(ElOffset, DEC);
      eeprom.write(30001, ElOffset);
      delay(200);
    }
    if (input == "AzB+") {                   // Azimuth Back +
      Serial2.println("");
      Serial2.print("AzBack = ");
      AzBack = AzBack + 1;
      Serial2.println(AzBack, DEC);
      eeprom.write(30002, AzBack);
      delay(200);
    }
    if (input == "AzB-") {                   // Azimuth Back -
      Serial2.println("");
      Serial2.print("AzBack = ");
      AzBack = AzBack - 1;
      Serial2.println(AzBack, DEC);
      eeprom.write(30002, AzBack);
      delay(200);
    }
    if (input == "ElB+") {                   // Elevation Back  +
      Serial2.println("");
      Serial2.print("ElBack= ");
      ElBack = ElBack + 1;
      Serial2.println(ElBack, DEC);
      eeprom.write(30003, ElBack);
      delay(200);
    }
    if (input == "ElB-") {                   // Elevation Offset -
      Serial2.println("");
      Serial2.print("ElBack= ");
      ElBack = ElBack - 1;
      Serial2.println(ElBack, DEC);
      eeprom.write(30003, ElBack);
      delay(200);
    }
    if (input == "Ang+") {                   // Elevation Back  +
      Serial2.println("");
      Serial2.print("Angl = ");
      Angl = Angl + 1;
      Serial2.println(Angl, DEC);
      eeprom.write(30004, Angl);
      delay(200);
    }
    if (input == "Ang-") {                   // Elevation Back  +
      Serial2.println("");
      Serial2.print("Angl = ");
      Angl = Angl - 1;
      Serial2.println(Angl, DEC);
      eeprom.write(30004, Angl);
      delay(200);
    }
    if (input == "Ctrl") {
      Serial2.println("");
      if (Clock.getDoW() == 1 ) {
        Serial2.print("Dimanche ");
      }
      if (Clock.getDoW() == 2 ) {
        Serial2.print("Lundi ");
      }
      if (Clock.getDoW() == 3 ) {
        Serial2.print("Mardi ");
      }
      if (Clock.getDoW() == 4 ) {
        Serial2.print("Mercredi ");
      }
      if (Clock.getDoW() == 5 ) {
        Serial2.print("Jeudi ");
      }
      if (Clock.getDoW() == 6 ) {
        Serial2.print("Vendredi ");
      }
      if (Clock.getDoW() == 7 ) {
        Serial2.print("Samedi ");
      }
      Serial2.print(Clock.getDate(), DEC);
      Serial2.print(" ");
      if (Clock.getMonth(Century) == 1 ) {
        Serial2.print("Janvier ");
      }
      if (Clock.getMonth(Century) == 2 ) {
        Serial2.print("Fevrier ");
      }
      if (Clock.getMonth(Century) == 3 ) {
        Serial2.print("Mars ");
      }
      if (Clock.getMonth(Century) == 4 ) {
        Serial2.print("Avril ");
      }
      if (Clock.getMonth(Century) == 5 ) {
        Serial2.print("Mai ");
      }
      if (Clock.getMonth(Century) == 6 ) {
        Serial2.print("Juin ");
      }
      if (Clock.getMonth(Century) == 7 ) {
        Serial2.print("Juillet ");
      }
      if (Clock.getMonth(Century) == 8 ) {
        Serial2.print("Aout ");
      }
      if (Clock.getMonth(Century) == 9 ) {
        Serial2.print("Septembre ");
      }
      if (Clock.getMonth(Century) == 10 ) {
        Serial2.print("Octobre ");
      }
      if (Clock.getMonth(Century) == 11 ) {
        Serial2.print("Novembre ");
      }
      if (Clock.getMonth(Century) == 12 ) {
        Serial2.print("Decembre ");
      }
      Serial2.print("20");
      Serial2.print(Clock.getYear(), DEC);
      Serial2.println("");
      Serial2.print(Clock.getHour(h12, PM), DEC);
      Serial2.print(":");
      Serial2.print(Clock.getMinute(), DEC);
      Serial2.print(":");
      Serial2.print(Clock.getSecond(), DEC);
      Serial2.print(" UT ");
      if (Status == 0)  {
        Serial2.print("Nuit ");
      }
      if (Status == 1)  {
        Serial2.print("Jour ");
      }
      Serial2.print("Temperature : ");
      Serial2.print(Clock.getTemperature(), 1);
      Serial2.println("C");
      Serial2.print("Azimuth  : ");
      Serial2.println(dAzimuth, 2);
      Serial2.print("Elevation: ");
      Serial2.println(dElevation, 2);
      Serial2.print("AzReach : ");
      Serial2.print(AzReach, 0);
      Serial2.print(" Offset ");
      Serial2.print(AzOffset);
      Serial2.print(" Back ");
      Serial2.println(AzBack);
      Serial2.print("ElReach : ");
      Serial2.print(ElReach, 0);
      Serial2.print(" Offset ");
      Serial2.print(ElOffset);
      Serial2.print(" Back ");
      Serial2.println(ElBack);
      Serial2.print("xReach : ");
      Serial2.println(xReach);
      Serial2.print("yReach : ");
      Serial2.println(yReach);
      Serial2.print("StartAngle : ");
      Serial2.println(Angl);
    }
    input = "";
  }
}
