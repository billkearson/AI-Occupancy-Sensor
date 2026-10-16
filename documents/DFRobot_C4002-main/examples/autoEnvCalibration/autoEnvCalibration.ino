/*!
 * @file autoEnvCalibration.ino
 * @brief This is an example to demonstrate how to use the DFRobot_C4002 library to perform environmental calibration.
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

// printDoorThreshold function
void printDoorThreshold(uint8_t *gateData, uint8_t n)
{
  Serial.print("Index:\t");
  for (int i = 0; i < n; i++) {
    Serial.print(i + 1);
    Serial.print('\t');
  }
  Serial.println();
  Serial.print("Value:\t");
  for (int i = 0; i < n; i++) {
    Serial.print(gateData[i]);
    Serial.print('\t');
  }
  Serial.println();
}

void setup()
{

  Serial.begin(115200);

  // Initialize the C4002 sensor
  while (c4002.begin() != true) {
    Serial.println("C4002 begin failed!");
    delay(1000);
  }
  Serial.println("C4002 begin success!");

  // Turn on the run led and out led
  if (c4002.setRunLedState(eLedOn)) {
    Serial.println("Set run led success!");
  } else {
    Serial.println("Set run led failed!");
  }
  delay(50);
  if (c4002.setOutLedState(eLedOn)) {
    Serial.println("Set out led success!");
  } else {
    Serial.println("Set out led failed!");
  }

  delay(3000);
  // Set the report period to 1s
  if (c4002.setReportPeriod(10)) {
    Serial.println("Set report period success!");
  } else {
    Serial.println("Set report period failed!");
  }
  /* note: Calibration and obtaining all data must have a set cycle */

  // Start environmental calibration
  // Delay time：10s ，Calibration time：30s( 15-65535 s )
  c4002.startEnvCalibration(10, 30);
  Serial.println("Start environmental calibration:");
  /**
   * Note:
   * 1. The calibration process takes about 30 seconds, and the delay time is 10 seconds.
   * 2. When resetting the development board, please find an open area to calibrate it
   * 3. When starting the calibration, there should be no one on either side of the sensor
   *  directly in front of the transmitter, otherwise it will affect the calibration accuracy
   *  of the sensor
   */
}

void loop()
{
  uint8_t           gateData[25] = { 0 };
  eResolutionMode_t resolutionMode;
  //Obtain the calibration results
  sRetResult_t retResult = c4002.getNoteInfo();

  if (retResult.noteType == eCalibration) {
    Serial.print("Calibration countdown:");
    Serial.print(retResult.calibCountdown);
    Serial.println(" s");
    if (retResult.calibCountdown == 0) {
      resolutionMode = c4002.getResolutionMode();
      int n          = resolutionMode == eResolution80Cm ? 15 : 25;
      Serial.println("************Environmental Calibration Complete****************");
      if (c4002.getDistanceGateThresh(eMotionDistGate, gateData)) {
        Serial.println("Motion distance gate threshold:");
        printDoorThreshold(gateData, n);
      } else {
        Serial.println("Get motion distance failed!");
      }
      if (c4002.getDistanceGateThresh(ePresenceDistGate, gateData)) {
        Serial.println("Presence distance gate threshold:");
        printDoorThreshold(gateData, n);
      } else {
        Serial.println("Get presence distance failed!");
      }
      Serial.println("**************************************************************");
    }
  }
}
