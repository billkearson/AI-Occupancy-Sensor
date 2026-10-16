/*!
 * @file getAllConfigs.ino
 * @brief This is an example to show how to get all configurations of the C4002 sensor.
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
DFRobot_C4002 c4002(&Serial1, 115200, /*D2*/ D2, /*D3*/ D3);
#else
DFRobot_C4002 c4002(&Serial1, 115200);
#endif

void setup()
{
  Serial.begin(115200);

  // Initialize the C4002 sensor
  while (c4002.begin() != true) {
    Serial.println("C4002 begin failed!");
    delay(1000);
  }
  Serial.println("C4002 begin success!");

  // Get all configurations of the C4002 sensor
  sConfigParams_t configParams = c4002.getAllConfigParams();
  // Print all configurations of the C4002 sensor
  Serial.print("Run LED: ");
  if (configParams.curLedStatus.runLed == eLedOn) {
    Serial.println("On");
  } else if (configParams.curLedStatus.runLed == eLedOff) {
    Serial.println("Off");
  }
  Serial.print("Out LED: ");
  if (configParams.curLedStatus.outLed == eLedOn) {
    Serial.println("On");
  } else if (configParams.curLedStatus.outLed == eLedOff) {
    Serial.println("Off");
  }

  Serial.print("Current light threshold:");
  Serial.print(configParams.curLightThreshold);
  Serial.println(" lux");

  Serial.print("Current detect range: ");
  Serial.print("( ");
  Serial.print(configParams.curDetectRange.closest);
  Serial.print(" , ");
  Serial.print(configParams.curDetectRange.farthest);
  Serial.println(" ) CM");

  Serial.print("Current output mode:Output Mode ");
  Serial.println(configParams.curOutMode);
  Serial.print("Current resolution mode:");
  if (configParams.curResolutionMode == eResolution80Cm) {
    Serial.println("80 cm");
    Serial.println("In this resolution mode, there are 15 operable distance gates, each with a range of 80cm");
  } else if (configParams.curResolutionMode == eResolution20Cm) {
    Serial.println("20 cm");
    Serial.println("In this resolution mode, there are 25 operable distance gates, each with a range of 20cm");
  }

  Serial.print("Current motion sensitivity:");
  if (eLowThreshGroup == configParams.curMotionSensitivity) {
    Serial.println("Low sensitivity");
  } else if (eMidThreshGroup == configParams.curMotionSensitivity) {
    Serial.println("Mid sensitivity");
  } else if (eHighThreshGroup == configParams.curMotionSensitivity) {
    Serial.println("High sensitivity");
  } else {
    Serial.println("Custom sensitivity");
  }

  Serial.print("Current presence sensitivity:");
  if (eLowThreshGroup == configParams.curPresenceSensitivity) {
    Serial.println("Low sensitivity");
  } else if (eMidThreshGroup == configParams.curPresenceSensitivity) {
    Serial.println("Mid sensitivity");
  } else if (eHighThreshGroup == configParams.curPresenceSensitivity) {
    Serial.println("High sensitivity");
  } else {
    Serial.println("Custom sensitivity");
  }
  Serial.print("Current target disappear delay time:");
  Serial.print(configParams.curTargetDisappearDelayTime);
  Serial.println(" s");
}

void loop()
{

  delay(10);
}
