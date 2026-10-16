# -*- coding: utf-8 -*
'''!
@file DFRobot_C4002.py
@brief Define the basic struct of DFRobot_C4002 class, the implementation of basic method
@copyright Copyright (c) 2025 DFRobot Co.Ltd (http://www.dfrobot.com)
@license The MIT License (MIT)
@author [JiaLi](zhixin.liu@dfrobot.com)
@version V1.0
@date 2025-11-04
@url https://github.com/DFRobot/DFRobot_C4002
'''

import time
import serial
import RPi.GPIO as GPIO
from ctypes import Structure, c_uint8, c_uint16, c_uint32, c_float, c_int16


class DetectResultStruct(Structure):
  _fields_ = [
    ("target_status", c_uint8),
    ("light", c_uint16),
    ("exist_gate_index", c_uint32),
    ("exist_count_down", c_uint16),
    ("exist_target_distance", c_uint16),
    ("exist_target_energy", c_uint8),
    ("move_target_distance", c_uint16),
    ("move_target_speed", c_int16),
    ("move_target_energy", c_uint8),
    ("move_target_direction", c_uint8),
  ]


class DataHeader(Structure):
  _fields_ = [("cmd", c_uint8), ("resp_code", c_uint8), ("data_len", c_uint16)]


class RecvPack(Structure):
  _fields_ = [("data_header", DataHeader), ("data", c_uint8 * 50), ("pack_type", c_uint8), ("result_pon_code", c_uint8)]


class ResData(Structure):
  _fields_ = [("cmd", c_uint8), ("resp_code", c_uint8), ("dect_result", DetectResultStruct), ("calibration_down", c_uint16)]


class PresenceTarget(Structure):
  _fields_ = [("distance", c_float), ("energy", c_uint8)]


class MotionTarget(Structure):
  _fields_ = [("distance", c_float), ("speed", c_float), ("energy", c_uint8), ("direction", c_uint8)]


class RetResult(Structure):
  _fields_ = [("note_type", c_uint8), ("calibration_down", c_uint16)]


class DetectRange(Structure):
  _fields_ = [("closest", c_uint16), ("farthest", c_uint16)]


class LedStatus(Structure):
  _fields_ = [("run_led", c_uint8), ("out_led", c_uint8)]


class ConfigParams(Structure):
  _fields_ = [
    ("cur_light_threshold", c_float),
    ("cur_detect_range", DetectRange),
    ("cur_out_mode", c_uint8),
    ("cur_resolution_mode", c_uint8),
    ("cur_motion_sensitivity", c_uint8),
    ("cur_presence_sensitivity", c_uint8),
    ("cur_target_disappe_delay_time", c_uint16),
    ("cur_led_status", LedStatus),
  ]


class DFRobot_C4002(object):
  TIME_OUT = 0x64  # time out
  FRAME_HEADER1 = 0xFA  # frame header1
  FRAME_HEADER2 = 0xF5  # frame header2
  FRAME_HEADER3 = 0xAA  # frame header3
  FRAME_HEADER4 = 0xA5  # frame header4
  FRAME_TYPE_WRITE_REQUSET = 0x00  # write request frame type
  FRAME_TYPE_READ_REQUSET = 0x01  # read request frame type
  FRAME_TYPE_WRITE_RESPOND = 0x02  # write respond frame type
  FRAME_TYPE_READ_RESPOND = 0x03  # read respond frame type
  FRAME_TYPE_NOTIFICATION = 0x04  # notification frame type
  FRAME_ERROR = 0xFF  # error frame type
  CMD_SET_LED_MODE = 0xA1  # set led mode
  CMD_CONFIG_OUT_MODE = 0xA0  # set output mode
  CMD_ENVIRNMENT_CALIBRATION = 0x60  # environment calibration
  CMD_RESTART = 0x00  # restart command
  CMD_SET_DETECT_RANGE = 0x86  # set detect sensitivity
  CMD_FACTORY_RESET = 0x80  # factory reset command
  CMD_FACTORY_RESET_USER = 0x02  # factory reset user command
  CMD_SET_REPORT_PERIOD = 0x83  # set report period
  CMD_SET_LIGHT_THRESHOLD = 0x88  # set light threshold
  CMD_SET_DISTANCE_DOOR = 0x62  # set distance gate
  CMD_GET_VERSION = 0x82  # get version command
  CMD_GET_AND_SET_RESOLUTION_MODE = 0x66  # get resolution mode command
  CMD_SET_DISTANCE_DOOR_THRESHOLD = 0x63  # set distance gate threshold
  CMD_SET_BAUDRATE = 0x21  # set baudrate command
  NOTE_RESULT_CMD = 0x60  # detection result notification command
  NOTE_ENVIRNMENT_CALIBRATION_CMD = 0x03  # environment calibration notification command
  SOFTWARE_VERSION = 0x01  # get software version
  HARDWARE_VERSION = 0x00  # get hardware version

  C4002_ENABLE = 1
  C4002_DISABLE = 0

  CMD_TARGET_DISAPPEAR_DELAY_TIME = 0x84  # set target disappear delay time
  CMD_LOCK_TIME = 0x85  # set lock time
  CMD_THRESHOLD_GROUP = 0x87  # set threshold group

  RESOLUTION_80CM = 0x00
  RESOLUTION_20CM = 0x01

  MOTION_DISTANCE_GATE = 0x00
  PRESENCE_DISTANCE_GATE = 0x01

  READ_AND_WRITE_REQ = 0x00
  SUCCEED = 0x01
  CMD_ERR = 0x02
  AUTHENTICATION_ERR = 0x03
  RESOURCES_BUSY = 0x04
  PARAMS_ERR = 0x05
  DATA_LEN_ERR = 0x06
  INTERNAL_ERR = 0x07

  OUT_PIN_MODE1 = 0x01
  OUT_PIN_MODE2 = 0x02
  OUT_PIN_MODE3 = 0x03

  AWAY = 0
  NODIRECTION = 1
  APPROACHING = 2

  NO_TARGET = 0
  PRESENCE = 1
  MOTION = 2
  MOTION_OR_PRESENCE = 3
  MOTION_OR_NO_TARGET = 4
  PRESENCE_OR_NO_TARGET = 5
  PIN_ERROR = 255

  LED_OFF = 0x00
  LED_ON = 0x01
  LED_KEEP = 0xFF

  NO_NOTE = 0x00
  RESULT = 0x01
  CALIBRATION = 0x02

  LOW_THRESH_GROUP = 0x00
  MID_THRESH_GROUP = 0x01
  HIGH_THRESH_GROUP = 0x02
  CUSTOM_THRESH_GROUP = 0x03
  CURRENT_THRESH_GROUP = 0xFF
  THRESH_GROUP_ERROR = 0xFE

  _out_mode = OUT_PIN_MODE3
  _resolution_mode = RESOLUTION_80CM
  _detect_result = DetectResultStruct()
  _outpin = 255

  def __init__(self, baud):
    self.ser = serial.Serial('/dev/ttyAMA0', baudrate=baud, bytesize=8, parity="N", stopbits=1, timeout=0.5)
    if self.ser.isOpen() == False:
      self.ser.open()

  def begin(self, outpin=255):
    '''!
    @brief begin
    param outpin: output pin, default is 255, which means no output pin is used.
    @return True or False
    '''
    ret = False
    time.sleep(0.5)
    if outpin != 255:
      self._outpin = outpin
      GPIO.setmode(GPIO.BCM)
      GPIO.setup(self._outpin, GPIO.IN)
    ret = self.set_report_period(255)
    if ret == False:
      return ret
    self._out_mode = self.get_out_pin_mode()
    self._resolution_mode = self.get_resolution_mode()
    return True

  def set_run_led_state(self, run_led):
    '''!
    @brief set run led state
    @param switching LED_OFF:off LED_ON:on LED_KEEP:keep
    @return True or False
    '''
    send_data = [0x00] * 10
    data_len = 6
    ret = False

    send_data[0] = self.CMD_SET_LED_MODE
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = run_led & 0xFF
    send_data[5] = self.LED_KEEP

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = True
    else:
      ret = False
    return ret

  def set_out_led_state(self, out_led):
    '''!
    @brief set out led state
    @param outled LED_OFF:off LED_ON:on LED_KEEP:keep
    @return True or False
    '''
    send_data = [0x00] * 10
    data_len = 6
    ret = False

    send_data[0] = self.CMD_SET_LED_MODE
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = self.LED_KEEP
    send_data[5] = out_led & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = True
    else:
      ret = False
    return ret

  def set_out_pin_mode(self, out_mode):
    '''!
    @brief set output pin mode
    @param out_mode
    @n  OUT_PIN_MODE1:Only when motion is detected will a high level be output.
    @n  OUT_PIN_MODE2:A high level is output only when its presence is detected.
    @n  OUT_PIN_MODE3:A high level only appears when motion or presence is detected.
    @return True or False
    '''
    send_data = [0x00] * 10
    data_len = 5
    ret = False
    send_data[0] = self.CMD_CONFIG_OUT_MODE
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = out_mode & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      self._out_mode = out_mode
      ret = True
    else:
      ret = False
    return ret

  def start_env_calibration(self, delay_time, cont_time):
    '''!
    @brief start environment calibration
    @param delay_time: delay time.range:0-65535s
    @param cont_time : continuous time.range:15-65535s
    '''
    send_data = [0x00] * 10
    data_len = 9

    send_data[0] = self.CMD_ENVIRNMENT_CALIBRATION
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = delay_time >> 0 & 0xFF
    send_data[5] = delay_time >> 8 & 0xFF
    send_data[6] = cont_time >> 0 & 0xFF
    send_data[7] = cont_time >> 8 & 0xFF
    send_data[8] = 0x01

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    self.recv_pack()

  def set_detect_range(self, closet_distance, farthest_distance):
    '''!
    @brief set detect range,unit:cm,range:0-1100cm
    @param closet_distance   : recent distance
    @param farthest_distance : farthest distance
    @return True or False
    '''
    send_data = [0x00] * 10
    data_len = 8
    ret = False
    closet = closet_distance
    farthest = farthest_distance

    send_data[0] = self.CMD_SET_DETECT_RANGE
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    if closet > farthest:
      ret = False
      return ret
    if farthest > 1100:
      farthest = 1100
    if closet < 0:
      closet = 0

    send_data[4] = closet >> 0 & 0xFF
    send_data[5] = closet >> 8 & 0xFF
    send_data[6] = farthest >> 0 & 0xFF
    send_data[7] = farthest >> 8 & 0xFF
    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = True
    else:
      ret = False
    return ret

  def configure_gate(self, gate_type, gate_data):
    '''!
    @brief configure distance gate
    @param gate_type distance gate type
    @param gate_data distance gate data
    @return True or False
    '''
    send_data = [0x00] * 40
    data_len = 5
    ret = False
    gate_num = 0
    if self._resolution_mode is self.RESOLUTION_80CM:
      gate_num = 15
    elif self._resolution_mode is self.RESOLUTION_20CM:
      gate_num = 25

    data_len = data_len + gate_num

    send_data[0] = self.CMD_SET_DISTANCE_DOOR
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = gate_type & 0xFF
    for i in range(gate_num):
      send_data[5 + i] = gate_data[i]

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = True
    else:
      ret = False
    return ret

  def factory_reset(self):
    '''!
    @brief factory reset,Restart takes effect
    @return True or False
    '''
    send_data = [0x00] * 10
    data_len = 5

    send_data[0] = self.CMD_FACTORY_RESET_USER
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = 0x00
    time.sleep(0.05)
    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()
    if recv_data.result_pon_code != self.SUCCEED:
      return False
    time.sleep(0.05)

    send_data[0] = self.CMD_FACTORY_RESET
    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()
    if recv_data.result_pon_code != self.SUCCEED:
      return False
    time.sleep(0.05)

    self.restart()

    return True

  def set_resolution_mode(self, resolution_mode):
    '''!
    @brief set resolution mode
    @param resolution_mode: resolution mode
    @n              RESOLUTION_80CM: 80cm resolution, supports up to 15 distance gates
    @n              RESOLUTION_20CM: 20cm resolution, supports up to 25 distance gates
    @return True or False
    '''
    send_data = [0x00] * 10
    data_len = 5
    ret = False

    send_data[0] = self.CMD_GET_AND_SET_RESOLUTION_MODE
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = resolution_mode & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      self._resolution_mode = resolution_mode
      ret = True
    else:
      ret = False
    return ret

  def set_report_period(self, period):
    '''!
    @brief set report period
    @param period report period,unit:0.1s
    @return True or False
    '''
    send_data = [0x00] * 10
    data_len = 5
    ret = False

    send_data[0] = self.CMD_SET_REPORT_PERIOD
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = period & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = True
    else:
      ret = False
    return ret

  def set_light_thresh(self, threshold):
    '''!
    @brief set light threshold
    @param threshold light threshold, range: 0-50, unit: lux
    @n     Note: When the valve group is set to 0, the target detection function will be triggered regardless of the light intensity.
    @n     When the threshold is not 0, the target inspection function will only be activated when the light intensity is lower than the threshold; otherwise, it will not be activated
    @return True or False
    '''
    send_data = [0x00] * 10
    data_len = 6
    ret = False

    send_data[0] = self.CMD_SET_LIGHT_THRESHOLD
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    threshold_temp = int(threshold * 10)
    send_data[4] = threshold_temp >> 0 & 0xFF
    send_data[5] = threshold_temp >> 8 & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = True
    else:
      ret = False
    return ret

  def set_gate_thresh(self, gate_type, thresh):
    '''!
    @brief set gate threshold
    @param gate_type: gate type
    @n              MOTION_DISTANCE_GATE:motion distance gate
    @n              PRESENCE_DISTANCE_GATE:presence distance gate
    @param thresh: Threshold, this is an array. The length of the array depends
    @n                on the resolution mode. In the 80cm mode, 15 gates can be set,
    @n                and in the 20cm mode, 25 gates can be set
    @return True or False
    '''
    send_data = [0x00] * 35
    data_len = 7
    ret = False
    gate_num = 0
    gate_index = 0x03
    if self._resolution_mode is self.RESOLUTION_80CM:
      gate_num = 15
    elif self._resolution_mode is self.RESOLUTION_20CM:
      gate_num = 25
      gate_indux = 0x04

    data_len = data_len + gate_num

    send_data[0] = self.CMD_SET_DISTANCE_DOOR_THRESHOLD
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = gate_type & 0xFF
    send_data[5] = gate_index & 0xFF
    send_data[6] = 0x01
    for i in range(gate_num):
      send_data[7 + i] = thresh[i]

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = True
    else:
      ret = False
    return ret

  def set_baudrate(self, baudrate):
    '''!
    @brief set baudrate
    @param baudrate baudrate
    @n     Note: It takes effect after a successful restart
    @n     The baud rate should not be too high; otherwise, it will lead to data loss
    @return True or False
    '''
    send_data = [0x00] * 10
    data_len = 8
    ret = False

    send_data[0] = self.CMD_SET_BAUDRATE
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = baudrate >> 0 & 0xFF
    send_data[5] = baudrate >> 8 & 0xFF
    send_data[6] = baudrate >> 16 & 0xFF
    send_data[7] = baudrate >> 24 & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = True
    else:
      ret = False
    return ret

  def get_note_info(self):
    '''!
    @brief get note data info
    '''
    ret_result = RetResult()
    rec_data = self.recv_pack()

    if rec_data.result_pon_code == self.SUCCEED:
      if rec_data.pack_type == self.FRAME_TYPE_NOTIFICATION:
        if rec_data.data_header.cmd == self.NOTE_RESULT_CMD:
          self._detect_result.target_status = rec_data.data[0]
          self._detect_result.light = rec_data.data[1] | rec_data.data[2] << 8
          self._detect_result.exist_gate_index = rec_data.data[3] | (rec_data.data[4] << 8) | (rec_data.data[5] << 16) | (rec_data.data[6] << 24)
          self._detect_result.exist_count_down = rec_data.data[7] | (rec_data.data[8] << 8)
          self._detect_result.exist_target_distance = rec_data.data[9] | (rec_data.data[10] << 8)
          self._detect_result.exist_target_energy = rec_data.data[11]
          self._detect_result.move_target_distance = rec_data.data[12] | (rec_data.data[13] << 8)
          self._detect_result.move_target_speed = rec_data.data[14] | (rec_data.data[15] << 8)
          self._detect_result.move_target_energy = rec_data.data[16]
          self._detect_result.move_target_direction = rec_data.data[17]

          ret_result.note_type = self.RESULT
        elif rec_data.data_header.cmd == self.NOTE_ENVIRNMENT_CALIBRATION_CMD:
          ret_result.calibration_down = rec_data.data[1] << 8 | rec_data.data[0]
          ret_result.note_type = self.CALIBRATION
        else:
          ret_result.note_type = self.NO_NOTE
    return ret_result

  def get_target_state(self):
    '''!
    @brief get target state
    @return detect result
    @n    NO_TARGET:no target
    @n    PRESENCE:presence
    @n    MOTION:motion
    '''
    return self._detect_result.target_status

  def get_light_intensity(self):
    '''!
    @brief Obtain light intensity,unit:lux
    '''
    return self._detect_result.light * 0.1

  def get_presence_gate_index(self):
    '''!
    @brief get presence gate index
    @n     Note: When the resolution mode is 80cm, 0 to 15 bits may be set to 1 to represent the presence of the target.
    @n     When the resolution mode is 20cm, 0 to 25 bits may be set to 1 to represent the presence of the target.
    @return The index of the detected target, range: 0-0xFFFFFFFF.
    '''
    return self._detect_result.exist_gate_index

  def get_presence_target_info(self):
    '''!
    @brief get presence target info
    @return PresenceTarget object
    @n          distance:distance,unit:m
    @n          energy:energy,range:0-100
    '''
    info = PresenceTarget()
    info.distance = self._detect_result.exist_target_distance * 0.01
    info.energy = self._detect_result.exist_target_energy
    return info

  def get_motion_target_info(self):
    '''!
    @brief get motion target info
    @return MotionTarget object
    @n          distance:distance,unit:m
    @n          speed:speed,unit:m/s
    @n          energy:energy,range:0-100
    @n          direction:direction,range:
    @n              AWAY:away
    @n              NODIRECTION:nondirectional
    @n              APPROACHING:near
    '''
    info = MotionTarget()
    info.distance = self._detect_result.move_target_distance * 0.01
    info.speed = self._detect_result.move_target_speed * 0.01
    info.energy = self._detect_result.move_target_energy
    info.direction = self._detect_result.move_target_direction
    return info

  def get_version_info(self, version):
    '''!
    @brief get version info
    @param version version type
    @n  VERSION_SOFTWARE:software version
    @n  VERSION_HARDWARE:hardware version
    @return string info
    '''
    send_data = [0x00] * 10
    data_len = 5
    ret = False
    ret_version = ""
    send_data[0] = self.CMD_GET_VERSION
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = version & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_READ_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      for i in range(recv_data.data_header.data_len):
        ret_version += chr(recv_data.data[i])
    else:
      ret_version = "Get version error!"

    return ret_version

  def get_out_target_state(self):
    '''!
    @brief get out target state
    @return out target state
    @n  NO_TARGET:no target
    @n  MOTION:motion
    @n  PRESENCE:presence
    @n  MOTION_OR_NO_TARGET:motion or no target
    @n  PRESENCE_OR_NO_TARGET:presence or no target
    @n  MOTION_OR_PRESENCE:motion or presence
    @n  PIN_ERROR:pin error
    '''
    ret = self.PIN_ERROR

    if self._outpin == 255:
      ret = self.PIN_ERROR
      return ret

    input_state = GPIO.input(self._outpin)

    if self._out_mode == self.OUT_PIN_MODE1:
      if input_state == GPIO.HIGH:
        ret = self.MOTION
      else:
        ret = self.PRESENCE_OR_NO_TARGET
    elif self._out_mode == self.OUT_PIN_MODE2:
      if input_state == GPIO.HIGH:
        ret = self.PRESENCE
      else:
        ret = self.MOTION_OR_NO_TARGET
    elif self._out_mode == self.OUT_PIN_MODE3:
      if input_state == GPIO.HIGH:
        ret = self.MOTION_OR_PRESENCE
      else:
        ret = self.NO_TARGET

    return ret

  def set_lock_time(self, lock_time):
    '''!
    @brief Set the lock time. When changing from occupied to unoccupied, the detection function
    @n   is locked for 1 second (default, adjustable). During the lock time, the sensor does not
    @n   detect targets. Detection is allowed again after the lock time expires.
    @param lock_time : The lock time, unit: s range:0.2-10s accuracy:0.1s
    @return True or False
    '''
    send_data = [0x00] * 10
    data_len = 6
    ret = False

    send_data[0] = self.CMD_LOCK_TIME
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    lock_time_temp = int(lock_time * 10)

    send_data[4] = lock_time_temp >> 0 & 0xFF
    send_data[5] = lock_time_temp >> 8 & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)
    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = True
    else:
      ret = False
    return ret

  def set_target_disappear_delay(self, disappear_time):
    '''!
    @brief Set the delay time for the target to disappear after it is no longer detected
    @param disappear_time : The delay time, unit: s (0-65535s)
    @return True or False
    '''
    send_data = [0x00] * 10
    data_len = 6
    ret = False

    send_data[0] = self.CMD_TARGET_DISAPPEAR_DELAY_TIME
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = disappear_time >> 0 & 0xFF
    send_data[5] = disappear_time >> 8 & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = True
    else:
      ret = False
    return ret

  def set_sensitivity(self, gate_type, sensitivity):
    '''!
    @brief Set the sensitivity of the detection
    @param gate_type : gate type
    @n  MOTION_DISTANCE_GATE:motion distance gate
    @n  PRESENCE_DISTANCE_GATE:presence distance gate
    @param sensitivity
    @n  LOW_THRESH_GROUP:low thresh group
    @n  MID_THRESH_GROUP:middle thresh group
    @n  HIGH_THRESH_GROUP:high thresh group
    @n  CUSTOM_THRESH_GROUP:custom thresh group
    @return True or False
    '''
    send_data = [0x00] * 10
    data_len = 6
    ret = False

    send_data[0] = self.CMD_THRESHOLD_GROUP
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = gate_type >> 0 & 0xFF
    send_data[5] = sensitivity >> 8 & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = True
    else:
      ret = False
    return ret

  def get_sensitivity(self, gate_type):
    '''!
    @brief get sensitivity
    @param gate_type gate type
    @n  MOTION_DISTANCE_GATE:motion distance gate
    @n  PRESENCE_DISTANCE_GATE:presence distance gate
    @return sensitivity
    '''
    send_data = [0x00] * 10
    data_len = 6
    sensitivity = 0
    send_data[0] = self.CMD_THRESHOLD_GROUP
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = gate_type >> 0 & 0xFF
    send_data[5] = 0x00

    self.send_pack(send_data, data_len, self.FRAME_TYPE_READ_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      sensitivity = recv_data.data[1]
    else:
      sensitivity = self.THRESH_GROUP_ERROR

    return sensitivity

  def get_light_thresh(self):
    '''!
    @brief get light threshold
    @return light threshold,unit:lux
    '''
    send_data = [0x00] * 10
    data_len = 4
    threshold = 0
    send_data[0] = self.CMD_SET_LIGHT_THRESHOLD
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_READ_REQUSET)
    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      threshold = recv_data.data[0] | recv_data.data[1] << 8

    return threshold

  def get_detect_range(self):
    '''!
    @brief get detect range
    @return DetectRange
    @n          closest:closest detect range, range:0-1100,unit:cm
    @n          farthest:farthest detect range, range:0-1100,unit:cm
    '''
    send_data = [0x00] * 10
    data_len = 4
    detect_range = DetectRange()
    send_data[0] = self.CMD_SET_DETECT_RANGE
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_READ_REQUSET)
    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      detect_range.closest = recv_data.data[0] | recv_data.data[1] << 8
      detect_range.farthest = recv_data.data[2] | recv_data.data[3] << 8

    return detect_range

  def get_target_disappear_delay(self):
    '''!
    @brief get target disappear delay
    @return target disappear delay,unit:s
    '''
    send_data = [0x00] * 10
    data_len = 4
    delay_time = 0
    send_data[0] = self.CMD_TARGET_DISAPPEAR_DELAY_TIME
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_READ_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      delay_time = recv_data.data[0] | recv_data.data[1] << 8

    return delay_time

  def get_all_config_params(self):
    '''!
    @brief get all config params
    @return ConfigParams object
    @n          cur_light_threshold:light threshold,unit:lux
    @n          cur_detect_range:detect range,unit:m
    @n          cur_target_disappe_delay_time:target disappear delay time,unit:s
    @n          cur_motion_sensitivity:motion sensitivity,range:0-100
    @n          cur_presence_sensitivity:presence sensitivity,range:0-100
    @n          cur_out_mode:out mode
    @n          cur_resolution_mode:resolution mode
    @n          cur_led_state:led state
    '''
    params = ConfigParams()
    params.cur_led_state = self.get_led_state()
    params.cur_light_threshold = self.get_light_thresh()
    params.cur_detect_range = self.get_detect_range()
    params.cur_target_disappe_delay_time = self.get_target_disappear_delay()
    params.cur_motion_sensitivity = self.get_sensitivity(self.MOTION_DISTANCE_GATE)
    params.cur_presence_sensitivity = self.get_sensitivity(self.PRESENCE_DISTANCE_GATE)
    params.cur_out_mode = self.get_out_pin_mode()
    params.cur_resolution_mode = self.get_resolution_mode()
    return params

  def get_presence_count_down(self):
    '''!
    @brief get the remaining time after target disappears, state changes from presence to no target
    @return the remaining time, unit:s
    '''
    return self._detect_result.exist_count_down

  def get_distance_gate_thresh(self, gate_type):
    '''!
    @brief get distance gate threshold
    @return distance gate threshold
    '''
    gate_threshold = [0x00] * 25
    send_data = [0x00] * 10
    data_len = 7
    send_data[0] = self.CMD_SET_DISTANCE_DOOR_THRESHOLD
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = gate_type >> 0 & 0xFF
    send_data[5] = self.CURRENT_THRESH_GROUP
    send_data[6] = 0x00
    recv_data = RecvPack()
    retry_count = 0
    max_retries = 5

    gate_num = 0
    if self._resolution_mode == self.RESOLUTION_80CM:
      gate_num = 15
    elif self._resolution_mode == self.RESOLUTION_20CM:
      gate_num = 25

    while recv_data.result_pon_code != self.SUCCEED and retry_count < max_retries:
      self.send_pack(send_data, data_len, self.FRAME_TYPE_READ_REQUSET)
      recv_data = self.recv_pack()

      if recv_data.result_pon_code == self.SUCCEED:
        for i in range(gate_num):
          gate_threshold[i] = recv_data.data[i + 3]
        return gate_threshold

      retry_count += 1
      time.sleep(0.05)

    return None

  def get_led_state(self):
    ret = LedStatus()
    send_data = [0x00] * 10
    data_len = 4
    send_data[0] = self.CMD_SET_LED_MODE
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_READ_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret.run_led = recv_data.data[0]
      ret.out_led = recv_data.data[1]
    else:
      ret.run_led = self.LED_KEEP
      ret.out_led = self.LED_KEEP

    return ret

  def restart(self):
    ret = 0
    send_data = [0x00] * 10
    data_len = 5

    send_data[0] = self.CMD_RESTART
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = 0x00

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = 0
    else:
      ret = -1
    return ret

  def get_out_pin_mode(self):
    send_pack = [0x00] * 10
    data_len = 4

    send_pack[0] = self.CMD_CONFIG_OUT_MODE
    send_pack[1] = self.READ_AND_WRITE_REQ
    send_pack[2] = data_len >> 0 & 0xFF
    send_pack[3] = data_len >> 8 & 0xFF

    self.send_pack(send_pack, data_len, self.FRAME_TYPE_READ_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      self._out_mode = recv_data.data[0]
      return self._out_mode
    else:
      return self._out_mode

  def get_resolution_mode(self):
    send_pack = [0x00] * 10
    data_len = 4

    send_pack[0] = self.CMD_GET_AND_SET_RESOLUTION_MODE
    send_pack[1] = self.READ_AND_WRITE_REQ
    send_pack[2] = data_len >> 0 & 0xFF
    send_pack[3] = data_len >> 8 & 0xFF

    self.send_pack(send_pack, data_len, self.FRAME_TYPE_READ_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      self._resolution_mode = recv_data.data[0]
      return self._resolution_mode
    else:
      return self._resolution_mode

  def set_led(self, run_led, out_led):
    send_data = [0x00] * 10
    data_len = 6
    ret = False

    send_data[0] = self.CMD_SET_LED_MODE
    send_data[1] = self.READ_AND_WRITE_REQ
    send_data[2] = data_len >> 0 & 0xFF
    send_data[3] = data_len >> 8 & 0xFF
    send_data[4] = run_led & 0xFF
    send_data[5] = out_led & 0xFF

    self.send_pack(send_data, data_len, self.FRAME_TYPE_WRITE_REQUSET)

    recv_data = self.recv_pack()

    if recv_data.result_pon_code == self.SUCCEED:
      ret = True
    else:
      ret = False
    return ret

  def send_pack(self, send_data, data_len, msg_type):
    send_pack = [0x00] * 50
    pack_len = 0x0000
    checksum = 0x0000
    send_pack[0] = self.FRAME_HEADER1
    pack_len += 1
    send_pack[1] = self.FRAME_HEADER2
    pack_len += 1
    send_pack[2] = self.FRAME_HEADER3
    pack_len += 1
    send_pack[3] = self.FRAME_HEADER4
    pack_len += 1
    temp = data_len + 10
    send_pack[4] = temp >> 0 & 0xFF
    pack_len += 1
    send_pack[5] = temp >> 8 & 0xFF
    pack_len += 1
    send_pack[6] = 0x00
    pack_len += 1
    send_pack[7] = msg_type
    pack_len += 1

    send_pack[pack_len : pack_len + data_len] = send_data
    pack_len += data_len
    checksum = self.get_check_sum(send_pack, pack_len)

    send_pack[pack_len] = checksum >> 0 & 0xFF
    pack_len += 1
    send_pack[pack_len] = checksum >> 8 & 0xFF
    pack_len += 1

    self.ser.flushInput()

    self.ser.write(send_pack[:pack_len])

  def recv_pack(self):
    recv_dat = RecvPack()
    recv_pack = [0x00] * 60

    recv_pack = self.read_reg(0, 8)

    if recv_pack[0] == self.FRAME_HEADER1 and recv_pack[1] == self.FRAME_HEADER2 and recv_pack[2] == self.FRAME_HEADER3 and recv_pack[3] == self.FRAME_HEADER4:
      packlen = (recv_pack[5] << 8) | recv_pack[4]

      recv_pack += bytearray(self.read_reg(0, packlen - 8))

      if len(recv_pack) == packlen:
        if self.check_sum(recv_pack, packlen):
          recv_dat.pack_type = recv_pack[7]
          recv_dat.data_header.cmd = recv_pack[8]
          recv_dat.data_header.resp_code = recv_pack[9]
          recv_dat.data_header.data_len = recv_pack[10] | (recv_pack[11] << 8)
          payload = recv_pack[12 : 12 + recv_dat.data_header.data_len]
          n = min(len(payload), 50)
          for i in range(50):
            recv_dat.data[i] = 0
          for i in range(n):
            recv_dat.data[i] = payload[i]
          recv_dat.result_pon_code = recv_dat.data_header.resp_code

          if not (recv_dat.pack_type == self.FRAME_TYPE_WRITE_RESPOND or recv_dat.pack_type == self.FRAME_TYPE_READ_RESPOND or recv_dat.pack_type == self.FRAME_TYPE_NOTIFICATION):
            recv_dat.result_pon_code = self.CMD_ERR
        else:
          recv_dat.result_pon_code = self.AUTHENTICATION_ERR
      else:
        recv_dat.result_pon_code = self.DATA_LEN_ERR
    else:
      recv_dat.result_pon_code = self.AUTHENTICATION_ERR
    return recv_dat

  def get_check_sum(self, send_data, data_len):
    parity = 0x0000
    for i in range(data_len):
      parity += send_data[i]
    return parity & 0xFFFF

  def check_sum(self, recv_data, data_len):
    parity = 0x0000
    for i in range(data_len - 2):
      parity += recv_data[i]
    temp = (recv_data[data_len - 1] << 8) | recv_data[data_len - 2]

    if temp == parity:
      return True
    return False

  def wrirte_reg(self, reg, data, length):
    send = bytes(data[:length])
    self.ser.flushInput()
    try:
      self.ser.write(send)
      return
    except:
      print("please check connect or mode!")
    return

  def read_reg(self, reg, length):
    recv = [0x00] * length
    timenow = time.time()
    while (time.time() - timenow) <= 0.1:
      count = self.ser.inWaiting()
      if count != 0:
        recv = self.ser.read(length)
        return recv
    return recv
