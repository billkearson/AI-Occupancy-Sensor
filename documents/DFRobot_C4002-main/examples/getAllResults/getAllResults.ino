/*!
 * @file getAllResults.ino
 * @brief This is an example to show how to use the DFRobot_C4002 library to get all the results of the C4002 sensor.
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
DFRobot_C4002 c4002(&Serial1, 115200, 17, 16);
#else
DFRobot_C4002 c4002(&Serial1, 115200);
#endif

int i;

void setup()
{
  Serial.begin(115200);

  // Initialize the C4002 sensor
  while (c4002.begin() != true) {
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

  // Set the out led to off
  if (c4002.setOutLedState(eLedOn)) {
    Serial.println("Set out led success!");
  } else {
    Serial.println("Set out led failed!");
  }
  delay(50);

    // Set the out led to off
  if (c4002.setOutPinMode(eOutpinMode2)) {
    Serial.println("Set outmode led success!");
  } else {
    Serial.println("Set outmode led failed!");
  }
  delay(50);

  // Set the Resolution mode to 80cm.
  if (c4002.setResolutionMode(eResolution80Cm)) {
    Serial.println("Set resolution mode success!");
  } else {
    Serial.println("Set resolution mode failed!");
  }
  delay(50);
  /**
   * Note:
   * 1. eResolution80Cm: This indicates that the resolution of the "distance gate" is 80cm.
   *  With a resolution of 80 cm, it supports up to 15 distance gates, with a maximum distance of 11.6 meters.
   * 2. eResolution20Cm: This indicates that the resolution of the "distance gate" is 20cm.
   *  With a resolution of 20 cm, it supports up to 25 distance gates, with a maximum distance of 4.9 meters
  */

  // Set the detect range to 0-1100 cm
  uint16_t clostRange = 0;
  uint16_t farRange   = 1100;
  if (c4002.setDetectRange(clostRange, farRange)) {    // Max detect range(0-1100cm)
    Serial.println("Set detect range success!");
  } else {
    Serial.println("Set detect range failed!");
  }
  delay(50);

  // Set the light threshold to 0 lux.range: 0-50 lux
  if (c4002.setLightThresh(0)) {
    Serial.println("Set light threshold success!");
  } else {
    Serial.println("Set light threshold failed!");
  }
  delay(50);

  /**
   * Note:
   *   If the effect of automatic environmental calibration is not good, you can use the manual setting of
   * the environmental threshold below, as shown below，
  */
  // // Set gate threshold to 50 ,range: 0-99
  // // Resolution mode:eResolution80Cm,This means that the number of 'distance gates' we can operate is 15
  // uint8_t presenceThreshold[15] = { 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50 };
  // uint8_t motionThreshold[15]  = { 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50 };
  // // Resolution mode:eResolution20Cm,This means that the number of 'distance gates' we can operate is 25
  // // uint8_t presenceThreshold[25] = {50,...,50,50};
  // // uint8_t motionThreshold[25]  = {50,...,50,50};
  // if (c4002.setGateThresh(ePresenceDistGate, presenceThreshold)) {
  //   Serial.println("Set presence gate threshold success!");
  // } else {
  //   Serial.println("Set presence gate threshold failed!");
  // }
  // delay(50);
  // if (c4002.setGateThresh(eMotionDistGate, motionThreshold)) {
  //   Serial.println("Set motion gate threshold success!");
  // } else {
  //   Serial.println("Set motion gate threshold failed!");
  // }
  // delay(50);

  // Enable the 'distance gate'
  // Resolution mode:eResolution80Cm,This means that the number of 'distance gates' we can operate is 15
  //disable : C4002_DISABLE, enable: C4002_ENABLE
  uint8_t gateState[15] = { C4002_ENABLE, C4002_ENABLE, C4002_ENABLE, C4002_ENABLE, C4002_ENABLE, C4002_ENABLE, C4002_ENABLE, C4002_ENABLE, C4002_ENABLE, C4002_ENABLE, C4002_ENABLE, C4002_ENABLE, C4002_ENABLE, C4002_ENABLE, C4002_ENABLE };
  // Resolution mode:eResolution20Cm,This means that the number of 'distance gates' we can operate is 25
  // uint8_t gateState[25] = {C4002_DISABLE,C4002_DISABLE,C4002_ENABLE,...,C4002_ENABLE,C4002_DISABLE};
  if (c4002.configureGate(eMotionDistGate, gateState)) {    // Operation motion distance gate
    Serial.println("Enable motion distance gate success!");
  }
  delay(50);
  if (c4002.configureGate(ePresenceDistGate, gateState)) {    // Operation presence distance gate
    Serial.println("Enable presence distance gate success!");
  }
  delay(50);

  // Set the target disappear delay time to 1s，range: 0-65535s
  if (c4002.setTargetDisappearDelay(1)) {
    Serial.println("Set target disappear delay time success!");
  } else {
    Serial.println("Set target disappear delay time failed!");
  }
  delay(50);

  // Set the report period to 10 * 0.1s = 1s
  if (c4002.setReportPeriod(10)) {
    Serial.println("Set report period success!");
  } else {
    Serial.println("Set report period failed!");
  }
  /* note: Calibration and obtaining all data must have a set cycle */
 
 i==-1;

}

void loop()
{
  // Get all the results of the C4002 sensor,Default loop execution
  sRetResult_t retResult = c4002.getNoteInfo();
  i++;
    
  Serial.println();

  if (retResult.noteType == eResult) {
    Serial.print(i);
    //Serial.println("------- Get all results --------");
    // get the light intensity
    float light = c4002.getLightIntensity();
    Serial.print(" Light:");
    Serial.print(light);
    //Serial.println(" lux");

    // get Target state
    eTargetState_t targetState = c4002.getTargetState();
    Serial.print(" TState:");
    if (targetState == eNoTarget) {
      Serial.print("No Target");
    } else if (targetState == ePresence) {
      Serial.print("Static");
    } else if (targetState == eMotion) {
      Serial.print("Motion");
    }

    // get presence count down
    //uint16_t presenceGateCount = c4002.getPresenceCountDown();
    //Serial.print("  PDistGC:");
    //Serial.print(presenceGateCount);
    //Serial.print(" s");

    // get Presence distance gate target info
    sPresenceTarget_t presenceTarget = c4002.getPresenceTargetInfo();
    Serial.print(" PDist:");
    Serial.print(presenceTarget.distance);
    //Serial.print(" m");
    Serial.print(" PEnergy:");
    Serial.print(presenceTarget.energy);

    // get motion distance gate index
    sMotionTarget_t motionTarget = c4002.getMotionTargetInfo();
    Serial.print(" MDistance:");
    Serial.print(motionTarget.distance);
    //Serial.print(" m");
    Serial.print(" MEnergy:");
    Serial.print(motionTarget.energy);
    //Serial.print(" MSpeed:");
    //Serial.print(motionTarget.speed);
    //Serial.print(" m/s");
    //Serial.print(" MDirection:");
    //if (motionTarget.direction == eAway) {
    //  Serial.print(" Away!");
    //} else if (motionTarget.direction == eNoDirection) {
    //  Serial.print("No Direction!");
    //} else if (motionTarget.direction == eApproaching) {
    //  Serial.print(" Approaching!");
    //}
    //Serial.println("--------------------------------");
  }
  else {
    Serial.print(retResult.noteType);
    //Serial.flush();
    sRetResult_t retResult = c4002.getNoteInfo();

  }


  delay(5000);
}
