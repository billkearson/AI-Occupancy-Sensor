# -*- coding: utf-8 -*
'''!
@file auto_env_calibration.py
@brief This demo can automatically calibrate the environmental parameters of the C4002 sensor.
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


def setup():
  while c4002.begin() != True:
    print("C4002 begin faild!")
    time.sleep(1)

  c4002.set_report_period(70)
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

  time.sleep(3)

  # Set the report period to 1s
  if c4002.set_report_period(10):
    print("Set report period success!")
  else:
    print("Set report period faild!")
  ## note: Calibration and obtaining all data must have a set cycle

  # Start environmental calibration
  # delay_time：10s ，Calibration_time：30s( 15-65535 s )
  c4002.start_env_calibration(10, 30)
  print("Start environmental calibration:")

  # Note:
  ## 1. The calibration process takes about 30 seconds, and the delay time is 10 seconds.
  ## 2. When resetting the development board, please find an open area to calibrate it
  ## 3. When starting the calibration, there should be no one on either side of the sensor
  ## directly in front of the transmitter, otherwise it will affect the calibration accuracy
  ## of the sensor


def loop():
  # Obtain the calibration results
  ret_result = c4002.get_note_info()
  gate_num = 0

  if ret_result.note_type == c4002.CALIBRATION:
    print("Calibration countdown:", ret_result.calibration_down, " s")
    if ret_result.calibration_down == 0:
      resolution_mode = c4002.get_resolution_mode()
      if resolution_mode == c4002.RESOLUTION_80CM:
        gate_num = 15
      elif resolution_mode == c4002.RESOLUTION_20CM:
        gate_num = 25

      resolution_mode = c4002._resolution_mode

      print("resolution mode:", resolution_mode)
      print("gate_num:", gate_num)

      time.sleep(0.1)
      print("************Environmental Calibration Complete****************")
      gate_threshold = c4002.get_distance_gate_thresh(c4002.MOTION_DISTANCE_GATE)
      if gate_threshold != None:
        print("Move distance gate threshold:")
        print("Index:", end="\t")
        for i in range(gate_num):
          print(i + 1, end="\t")
        print()
        print("Value:", end="\t")
        for i in range(gate_num):
          print(gate_threshold[i], end="\t")
        print()
      else:
        print("Get motion distance gate threshold faild!")

      time.sleep(0.1)
      gate_threshold = c4002.get_distance_gate_thresh(c4002.PRESENCE_DISTANCE_GATE)
      if gate_threshold != None:
        print("presence distance gate threshold:")
        print("Index:", end="\t")
        for i in range(gate_num):
          print(i + 1, end="\t")
        print()
        print("Value:", end="\t")
        for i in range(gate_num):
          print(gate_threshold[i], end="\t")
        print()
      else:
        print("Get presence distance gate threshold faild!")
      print("**************************************************************")

  time.sleep(0.3)


if __name__ == "__main__":
  setup()
  while True:
    loop()
