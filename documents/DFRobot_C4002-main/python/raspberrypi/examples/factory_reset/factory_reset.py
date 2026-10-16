# -*- coding: utf-8 -*
'''!
@file factory_reset.py
@brief This example shows how to restore factory Settings.
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

  time.sleep(2)

  # Restore factory settings
  print("Restore factory settings...")
  if c4002.factory_reset():
    print("Factory reset success!")
    print("After restoring the factory Settings, a restart is required!")
  else:
    print("Factory reset failed!")


def loop():
  time.sleep(1)


if __name__ == "__main__":
  setup()
  while True:
    loop()
