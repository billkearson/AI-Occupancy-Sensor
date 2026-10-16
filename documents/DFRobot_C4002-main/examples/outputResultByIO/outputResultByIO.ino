/*!
 * @file getAllResults.ino
 * @brief This is an example to get the output results of the C4002 sensor by the 'out' pin.
 * @copyright	Copyright (c) 2025 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @author JiaLi(zhixin.liu@dfrobot.com)
 * @version V1.0
 * @date 2025-11-04
 * @url https://github.com/DFRobot/DFRobot_C4002
 */
#include "DFRobot_C4002.h"

/* ---------------------------------------------------------------------------------------------------------------------
  *    board   |             MCU                | Leonardo/Mega2560/M0 |    UNO    | ESP8266 | ESP32 |  microbit  |   m0  |
  *     VCC    |              5V                |         5V           |     5V    |    5V   |   5V  |     X      |   5V  |
  *     GND    |              GND               |        GND           |    GND    |   GND   |  GND  |     X      |  GND  |
  *     RX     |              TX                |     Serial1 TX1      |     5     |   5/D6  | 25/D2 |     X      |  tx1  |
  *     TX     |              RX                |     Serial1 RX1      |     4     |   4/D7  | 26/D3 |     X      |  rx1  |
  * ----------------------------------------------------------------------------------------------------------------------*/
/* Baud rate can be changed */

#if defined(ESP8266) || defined(ARDUINO_AVR_UNO)
SoftwareSerial mySerial(4, 5);
DFRobot_C4002  c4002(&mySerial, 115200);
#elif defined(ESP32)
DFRobot_C4002 c4002(&Serial1, 115200, /* D2 */ D2, /* D3 */ D3);
#else
DFRobot_C4002 c4002(&Serial1, 115200);
#endif

uint8_t outPin = 6;
/**
  * Set the input pin numbers of the development board and connect the
  * 'out' pin out of the sensor to the designated development board card
  */
eOutpinMode_t outMode = eOutpinMode3;
/**
  * Note:
  * The output mode can be set to eOutpinMode1, eOutpinMode2, or eOutpinMode3.
  * eOutpinMode1: A high level will be output when motion is detected.
  * eOutpinMode2: A high level will be output when presence is detected.
  * eOutpinMode3: A high level will be output when motion or presence is detected.
  */

void setup()
{
  Serial.begin(115200);

  // Initialize the C4002 sensor
  while (c4002.begin(outPin) != true) {
    Serial.println("C4002 begin failed!");
    delay(1000);
  }
  Serial.println("C4002 begin success!");
  delay(50);

  // Set the run led to off
  if (c4002.setRunLedState(eLedOn)) {
    Serial.println("Set run led success!");
  } else {
    Serial.println("Set run led failed!");
  }
  delay(50);

  // Set the out led to off,blue led,It is connected in parallel with the 'out' pin of the sensor.
  if (c4002.setOutLedState(eLedOn)) {
    Serial.println("Set out led success!");
  } else {
    Serial.println("Set out led failed!");
  }
  delay(50);

  // Set the output pin mode to eOutpinMode3
  if (c4002.setOutPinMode(outMode)) {
    Serial.println("Set output mode success!");
  } else {
    Serial.println("Set output mode failed!");
  }
}

void loop()
{
  // Get the target state by the output pin
  eTargetState_t targetState = c4002.getOutTargetState();
  Serial.println("------- Get outpin results --------");

  Serial.print("Output mode:");
  if (outMode == eOutpinMode1) {
    Serial.println("Mode1");
  } else if (outMode == eOutpinMode2) {
    Serial.println("Mode2");
  } else if (outMode == eOutpinMode3) {
    Serial.println("Mode3");
  }
  //Read the outPin status
  uint8_t outPinStatus = digitalRead(outPin);
  Serial.print("Outpin :");
  if (outPinStatus == HIGH) {
    Serial.println(" high level!");
  } else {
    Serial.println("  low level!");
  }

  // Print the target state
  Serial.print("Target state: ");
  if (targetState == eNoTarget) {
    Serial.println("No Target!");
  } else if (targetState == ePresence) {
    Serial.println("Static Presence!");
  } else if (targetState == eMotion) {
    Serial.println("Motion!");
  } else if (targetState == eMotionOrNoTarget) {
    Serial.println("Motion or No Target!");
  } else if (targetState == ePresenceOrNoTarget) {
    Serial.println("Static Presence or No Target!");
  } else if (targetState == eMotionOrPresence) {
    Serial.println("Motion or Static Presence!");
  } else {
    Serial.println("Pin error! please check the connection or pin number!");
  }
  Serial.println("-----------------------------------");
  delay(500);
}
