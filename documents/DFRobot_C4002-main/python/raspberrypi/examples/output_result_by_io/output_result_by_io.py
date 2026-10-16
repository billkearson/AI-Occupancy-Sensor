# -*- coding: utf-8 -*
'''!
@file output_result_by_io.py
@brief Output the detection result by the output pin of the Raspberry PI.
@copyright Copyright (c) 2025 DFRobot Co.Ltd (http://www.dfrobot.com)
@license The MIT License (MIT)
@author [JiaLi](zhixin.liu@dfrobot.com)
@version V1.0
@date 2025-11-04
@url https://github.com/DFRobot/DFRobot_C4002
'''

import time
import sys

sys.path.append("../../")

from DFRobot_C4002 import *

c4002 = DFRobot_C4002(115200)

# output pin,BCM coding
out_pin = 18
## Set the input pins of the Raspberry PI，BCM coding
## Connect them to the C4002 sensor out pins
out_mode = c4002.OUT_PIN_MODE3
## The output mode can be set to eOutpinMode1, eOutpinMode2, or eOutpinMode3.
## eOutpinMode1: A high level will be output when motion is detected.
## eOutpinMode2: A high level will be output when presence is detected.
## eOutpinMode3: A high level will be output when motion or presence is detected.


def setup():
  while c4002.begin(out_pin) != True:
    print("C4002 begin faild!")
    time.sleep(1)

  print("C4002 begin success!")
  time.sleep(0.05)

  # Turn on the run led and out led
  if c4002.set_run_led_state(c4002.LED_ON):
    print("Set run led success!")
  else:
    print("Set run led faild!")
  time.sleep(0.05)
  if c4002.set_out_led_state(c4002.LED_ON):
    print("Set out led success!")
  else:
    print("Set out led faild!")
  time.sleep(0.05)

  # Set the output pin mode to output the detection result
  if c4002.set_out_pin_mode(out_mode):
    print("Set out mode success!")
  else:
    print("Set out mode faild!")
  time.sleep(0.05)


def loop():
  # Get the target state by the output pin
  target_state = c4002.get_out_target_state()

  print("------- Get outpin results --------")

  print("Output mode:", end="")
  if out_mode == c4002.OUT_PIN_MODE1:
    print("Mode1")
  elif out_mode == c4002.OUT_PIN_MODE2:
    print("Mode2")
  elif out_mode == c4002.OUT_PIN_MODE3:
    print("Mode3")

  print("Outpin :", end="")
  out_pin_state = GPIO.input(out_pin)
  if out_pin_state == 1:
    print(" high level!")
  else:
    print("  low level!")

  # Print the target state
  print("Target state: ", end="")
  if target_state == c4002.NO_TARGET:
    print("No Target!")
  elif target_state == c4002.MOTION:
    print("Motion!")
  elif target_state == c4002.PRESENCE:
    print("Static Presence!")
  elif target_state == c4002.MOTION_OR_PRESENCE:
    print("Motion Or Static Presence!")
  elif target_state == c4002.MOTION_OR_NO_TARGET:
    print("Motion Or No Target!")
  elif target_state == c4002.PRESENCE_OR_NO_TARGET:
    print("Static Presence Or No Target!")

  print("-----------------------------------")
  time.sleep(0.5)


if __name__ == "__main__":
  setup()
  while True:
    loop()
