# -*- coding: utf-8 -*
'''!
@file get_all_configs.py
@brief This example shows how to get all configurations of the C4002.
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

  time.sleep(1)
  # Get all configurations
  config_params = c4002.get_all_config_params()

  print("ConfigParams:")
  print("Run LED: ", end="")
  if config_params.cur_led_state.run_led == c4002.LED_OFF:
    print("OFF")
  elif config_params.cur_led_state.run_led == c4002.LED_ON:
    print("ON")
  print("Out LED: ", end="")
  if config_params.cur_led_state.out_led == c4002.LED_OFF:
    print("OFF")
  elif config_params.cur_led_state.out_led == c4002.LED_ON:
    print("ON")

  print("Current light threshold: ", config_params.cur_light_threshold, "lux")
  print("Current detect range: (", config_params.cur_detect_range.closest, " , ", config_params.cur_detect_range.farthest, " ) cm")
  print("Current out mode:  OUT MODE ", config_params.cur_out_mode)
  print("Current resolution mode: ", end="")
  if config_params.cur_resolution_mode == c4002.RESOLUTION_80CM:
    print("80 cm")
    print("In this resolution mode, there are 15 operable distance gates, each with a range of 80cm")
  else:
    print("20 cm")
    print("In this resolution mode, there are 25 operable distance gates, each with a range of 20cm")

  print("Current motion sensitivity: ", end="")
  if config_params.cur_motion_sensitivity == c4002.LOW_THRESH_GROUP:
    print("Low sensitivity")
  elif config_params.cur_motion_sensitivity == c4002.MID_THRESH_GROUP:
    print("Mid sensitivity")
  elif config_params.cur_motion_sensitivity == c4002.HIGH_THRESH_GROUP:
    print("High sensitivity")
  else:
    print("Custom sensitivity")

  print("Current presence sensitivity: ", end="")
  if config_params.cur_presence_sensitivity == c4002.LOW_THRESH_GROUP:
    print("Low sensitivity")
  elif config_params.cur_presence_sensitivity == c4002.MID_THRESH_GROUP:
    print("Mid sensitivity")
  elif config_params.cur_presence_sensitivity == c4002.HIGH_THRESH_GROUP:
    print("High sensitivity")
  else:
    print("Custom sensitivity")

  print("Current target disappear delay time: ", config_params.cur_target_disappe_delay_time, ' s')


def loop():
  time.sleep(1)


if __name__ == "__main__":
  setup()
  while True:
    loop()
