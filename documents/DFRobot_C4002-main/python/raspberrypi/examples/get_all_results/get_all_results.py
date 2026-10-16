# -*- coding: utf-8 -*
'''!
@file get_all_results.py
@brief This is an example to show how to use the DFRobot_C4002 library to get all the results of the C4002 sensor.
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
  print("C4002 begin success!")
  time.sleep(0.05)

  # Turn on the run led and out led
  if c4002.set_run_led_state(c4002.LED_OFF):
    print("Set run led success!")
  else:
    print("Set run led faild!")
  time.sleep(0.05)
  if c4002.set_out_led_state(c4002.LED_OFF):
    print("Set out led success!")
  else:
    print("Set out led faild!")
  time.sleep(0.05)

  if c4002.set_resolution_mode(c4002.RESOLUTION_80CM):
    print("Set resolution mode success!")
  else:
    print("Set resolution mode faild!")
  time.sleep(0.05)
  # Note
  ## 1.eResolution80Cm: This indicates that the resolution of the "distance gate" is 80cm.
  ## With a resolution of 80 cm, it supports up to 15 distance gates, with a maximum distance of 11.6 meters.
  ## 2.eCalibration: This indicates that the calibration is in progress.
  ## With a resolution of 20 cm, it supports up to 25 distance gates, with a maximum distance of 4.9 meters

  # Set the detect range to 0-1100 cm
  clost_range = 0
  far_range = 1100
  if c4002.set_detect_range(clost_range, far_range):  # Max detect range(0-1100cm)
    print("Set detect range success!")
  else:
    print("Set detect range faild!")
  time.sleep(0.05)

  # Set the light threshold to 0 lux.range: 0-50 lux
  if c4002.set_light_thresh(0):
    print("Set light threshold success!")
  else:
    print("Set light threshold faild!")
  time.sleep(0.05)

  ## Note
  #    If the effect of automatic environmental calibration is not good, you can use the manual setting of
  #  the environmental threshold below, as shown below:

  # # Set gate threshold to 50 ,range: 0-99
  # ## Resolution mode:eResolution80Cm,This means that the number of 'distance gates' we can operate is 15
  # presence_threshold = [50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50]  # 15 gates
  # motion_threshold = [50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50]  # 15 gates
  # ## Resolution mode:eResolution20Cm,This means that the number of 'distance gates' we can operate is 25
  # ## presence_threshold = [50, 50, 50,..., 50, 50, 50] #25 gates
  # ## motion_threshold  = [50, 50, 50,..., 50, 50, 50] #25 gates
  # if c4002.set_gate_thresh(c4002.PRESENCE_DISTANCE_GATE, presence_threshold):
  #   print("Set presence gate threshold success!")
  # else:
  #   print("Set presence gate threshold failed!")
  # time.sleep(0.05)
  # if c4002.set_gate_thresh(c4002.MOTION_DISTANCE_GATE, motion_threshold):
  #   print("Set motion gate threshold success!")
  # else:
  #   print("Set motion gate threshold failed!")
  # time.sleep(0.05)

  # Enable the 'distance gate'
  # Resolution mode:eResolution80Cm,This means that the number of 'distance gates' we can operate is 15
  # disable : C4002_DISABLE, enable: C4002_ENABLE
  gate_state = [
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
    c4002.C4002_ENABLE,
  ]
  # Resolution mode:eResolution20Cm,This means that the number of 'distance gates' we can operate is 25
  # gate_state = [c4002.C4002_DISABLE, c4002.C4002_DISABLE, c4002.C4002_ENABLE, ..., c4002.C4002_ENABLE, c4002.C4002_DISABLE]
  if c4002.configure_gate(c4002.MOTION_DISTANCE_GATE, gate_state):  # Operation motion distance gate
    print("Enable motion distance gate success!")
  time.sleep(0.05)
  if c4002.configure_gate(c4002.PRESENCE_DISTANCE_GATE, gate_state):  # Operation presence distance gate
    print("Enable presence distance gate success!")
  time.sleep(0.05)

  # set the report period to 1s
  if c4002.set_target_disappear_delay:
    print("Set target disappear delay success!")
  else:
    print("Set target disappear delay failed!")
  time.sleep(0.05)

  # Set the report period to 10 * 0.1s = 1s
  if c4002.set_report_period(10):
    print("Set report period success!")
  else:
    print("Set report period failed!")
  time.sleep(0.05)
  ## note: Calibration and obtaining all data must have a set cycle


def loop():
  # Obtain the calibration results
  ret_result = c4002.get_note_info()

  if ret_result.note_type == c4002.RESULT:
    print("------- Get all results --------")
    # get the light intensity
    light = c4002.get_light_intensity()
    print("Light: ", round(light, 2), " lux")

    # get Target state
    target_state = c4002.get_target_state()
    print("Target state: ", end="")
    if target_state == c4002.NO_TARGET:
      print("No Target")
    elif target_state == c4002.PRESENCE:
      print("Static Presence")
    elif target_state == c4002.MOTION:
      print("Motion")

    # get presence count down
    presence_count_down = c4002.get_presence_count_down()
    print("Presence distance gate count down: ", presence_count_down, ' s')

    # get presence distance gate target info
    presence_target = PresenceTarget()
    presence_target = c4002.get_presence_target_info()
    print("Presence distance: ", round(presence_target.distance, 2), " m")
    print("Presence energy: ", presence_target.energy)

    # get motion distance gate index
    motion_target = MotionTarget()
    motion_target = c4002.get_motion_target_info()
    print("Motion distance: ", round(motion_target.distance, 2), " m")
    print("Motion energy: ", motion_target.energy)
    print("motion speed: ", round(motion_target.speed, 2), " m/s")
    print("motion direction: ", end="")
    if motion_target.direction == c4002.AWAY:
      print("Away!")
    elif motion_target.direction == c4002.APPROACHING:
      print("Approaching!")
    elif motion_target.direction == c4002.NODIRECTION:
      print("No Direction!")
    else:
      print("Unknown direction.")
    print("--------------------------------")


if __name__ == "__main__":
  setup()
  while True:
    loop()
