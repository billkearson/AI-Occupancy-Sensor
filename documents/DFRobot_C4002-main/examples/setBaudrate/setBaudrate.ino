/*!
 * @file setBaudrate.ino
 * @brief This is an example to set the baudrate of the C4002 sensor.
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

unsigned long curBaudrate = 115200;

#if defined(ESP8266) || defined(ARDUINO_AVR_UNO)
SoftwareSerial mySerial(4, 5);
DFRobot_C4002  c4002(&mySerial, curBaudrate);
#elif defined(ESP32)
DFRobot_C4002 c4002(&Serial1, curBaudrate, /* D2 */ D2, /* D3 */ D3);
#else
DFRobot_C4002 c4002(&Serial1, curBaudrate);
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
  delay(50);

  // Set the baudrate
  /* Note:The default baud rate is 115200.
  Note: Setting the baud rate too low may result in data transmission loss or data loss. */
  eBaudrate_t setBaudrate = eBaud115200;    // eBaud57600, eBaud115200, eBaud230400...
  if (c4002.setBaudrate(setBaudrate)) {
    Serial.println("Set baudrate success!");
    Serial.println("After setting the baudrate, a restart is required!");
  } else {
    Serial.println("Set baudrate failed!");
  }
}

void loop()
{
  delay(100);
}
