/*
  AI Occupancy Sensor
  ESP32-C6 + C4002 mmWave Sensor + ENS160 + BME280
  Monitors motion, presence, room occupancy, and air quality.
  Includes WiFi connectivity, web server, MCP, and MQTT reporting.

  Project: AI Occupancy Sensor
  Author: Bill
  Date: 2026-09-21
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <stdlib.h>
#include <time.h>
#include "config.h"

// Pin assignments for the ESP32-C6 board and connected sensors.
constexpr uint8_t C4002_UART_RX = 17;
constexpr uint8_t C4002_UART_TX = 16;
constexpr uint8_t I2C_SDA = 19;
constexpr uint8_t I2C_SCL = 20;

constexpr HTTPMethod HTTP_METHOD_GET = static_cast<HTTPMethod>(1);
constexpr HTTPMethod HTTP_METHOD_POST = static_cast<HTTPMethod>(3);

// Latest parsed values from the C4002 mmWave occupancy sensor.
struct C4002State {
  bool valid = false;
  uint8_t note_type = 0;
  uint8_t target_state = 0;
  uint16_t distance_cm = 0;
  uint8_t energy = 0;
  bool motion = false;
  bool presence = false;
  uint16_t signal_strength = 0;
};

// Normalized, room-level view of the current sensor readings.
struct SensorSnapshot {
  bool c4002_present = false;
  bool motion_detected = false;
  bool static_presence = false;
  float distance_m = 0.0f;
  int energy_level = 0;
  bool ens160_ready = false;
  float eco2_ppm = 0.0f;
  float tvoc_ppb = 0.0f;
  bool bme280_ready = false;
  float temperature_c = 0.0f;
  float humidity_rh = 0.0f;
  float pressure_hpa = 0.0f;
  uint64_t timestamp_ms = 0;
};

struct RoomContext {
  bool occupied = false;
  bool moving = false;
  bool stationary = false;
  bool air_quality_warning = false;
  bool ventilation_recommended = false;
  float occupancy_duration_s = 0.0f;
  float temperature_c = 0.0f;
  float humidity_rh = 0.0f;
  float eco2_ppm = 0.0f;
  float tvoc_ppb = 0.0f;
};

struct OccupancyHistoryEntry {
  uint64_t timestamp_ms = 0;
  bool time_source_ntp = false;
  bool occupied = false;
  bool moving = false;
  bool stationary = false;
  float distance_ft = 0.0f;
  int energy = 0;
};

constexpr size_t OCCUPANCY_HISTORY_DEPTH = 10;

// Driver for the DFRobot C4002 mmWave sensor.
// It wraps the UART packet framing, command payloads, and notification parsing
// required to read motion/presence and distance updates from the sensor.
class C4002Driver {
public:
  static constexpr uint8_t FRAME_HEADER_1 = 0xFA;
  static constexpr uint8_t FRAME_HEADER_2 = 0xF5;
  static constexpr uint8_t FRAME_HEADER_3 = 0xAA;
  static constexpr uint8_t FRAME_HEADER_4 = 0xA5;
  static constexpr uint8_t FRAME_TYPE_WRITE_REQUEST = 0x00;
  static constexpr uint8_t FRAME_TYPE_READ_REQUEST = 0x01;
  static constexpr uint8_t FRAME_TYPE_NOTIFICATION = 0x04;
  static constexpr uint8_t FRAME_TYPE_READ_RESPONSE = 0x03;
  static constexpr uint8_t CMD_SET_LED_MODE = 0xA1;
  static constexpr uint8_t CMD_CONFIG_OUT_MODE = 0xA0;
  static constexpr uint8_t CMD_GET_AND_SET_RESOLUTION_MODE = 0x66;
  static constexpr uint8_t CMD_SET_REPORT_PERIOD = 0x83;
  static constexpr uint8_t CMD_SET_LIGHT_THRESHOLD = 0x88;
  static constexpr uint8_t CMD_SET_DETECT_RANGE = 0x86;
  static constexpr uint8_t CMD_SET_DISTANCE_DOOR = 0x62;
  static constexpr uint8_t CMD_TARGET_DISAPPEAR_DELAY = 0x84;
  static constexpr uint8_t SUCCEED = 0x01;
  static constexpr uint8_t NO_TARGET = 0;
  static constexpr uint8_t PRESENCE = 1;
  static constexpr uint8_t MOTION = 2;

  enum LedMode : uint8_t {
    LED_OFF = 0x00,
    LED_ON = 0x01,
    LED_KEEP = 0xFF,
  };

  enum ResolutionMode : uint8_t {
    RESOLUTION_80CM = 0x00,
    RESOLUTION_20CM = 0x01,
  };

  enum OutPinMode : uint8_t {
    OUT_PIN_MODE1 = 0x01,
    OUT_PIN_MODE2 = 0x02,
    OUT_PIN_MODE3 = 0x03,
  };

  struct Diagnostics {
    uint32_t packets_read = 0;
    uint32_t notifications_seen = 0;
    uint32_t result_notes_parsed = 0;
    uint32_t calibration_notes_seen = 0;
    uint32_t rejected_bad_len = 0;
    uint32_t rejected_bad_resp = 0;
    uint32_t rejected_unknown_note = 0;
  };

  // Initialize the UART connection to the C4002 sensor and store the serial settings.
  bool begin(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin, uint32_t baudRate) {
    serial_ = &serial;
    rxPin_ = rxPin;
    txPin_ = txPin;
    baudRate_ = baudRate;
    serial_->begin(baudRate_, SERIAL_8N1, rxPin_, txPin_);
    delay(50);
    return true;
  }

  // Reset the C4002 to a known-good configuration after a stale or invalid reading.
  bool reinitialize() {
    if (serial_ == nullptr) {
      return false;
    }

    serial_->flush();
    serial_->end();
    delay(150);
    serial_->begin(baudRate_, SERIAL_8N1, rxPin_, txPin_);
    delay(250);

    const uint8_t gateState15[15] = {
      SUCCEED, SUCCEED, SUCCEED, SUCCEED, SUCCEED,
      SUCCEED, SUCCEED, SUCCEED, SUCCEED, SUCCEED,
      SUCCEED, SUCCEED, SUCCEED, SUCCEED, SUCCEED,
    };

    bool ok = true;
    ok = setRunLedState(LED_ON) && ok;
    ok = setOutLedState(LED_ON) && ok;
    ok = setResolutionMode(RESOLUTION_80CM) && ok;
    ok = setOutPinMode(OUT_PIN_MODE3) && ok;
    ok = setLightThresh(0.0f) && ok;
    ok = configureGate(0x00, gateState15, 15) && ok;
    ok = configureGate(0x01, gateState15, 15) && ok;
    ok = setTargetDisappearDelay(60) && ok;
    ok = setDetectRange(0, 1100) && ok;
    ok = setReportPeriod(50) && ok;

    return ok;
  }

  // Detect a sensor state that looks like a reset or invalid all-zero packet instead of a real reading.
  static bool isZeroedState(const C4002State &state) {
    return state.valid && !state.motion && !state.presence && state.distance_cm == 0 && state.energy == 0;
  }

  // Configure how often the C4002 emits report frames to the UART bus.
  bool setReportPeriod(uint8_t period) {
    uint8_t payload[5] = {CMD_SET_REPORT_PERIOD, 0x00, 0x05, 0x00, period};
    return sendCommand(payload, sizeof(payload), FRAME_TYPE_WRITE_REQUEST);
  }

  // Set the C4002 ranging resolution for the configured detection distance window.
  bool setResolutionMode(ResolutionMode mode) {
    uint8_t payload[5] = {CMD_GET_AND_SET_RESOLUTION_MODE, 0x00, 0x05, 0x00, uint8_t(mode)};
    return sendCommand(payload, sizeof(payload), FRAME_TYPE_WRITE_REQUEST);
  }

  // Turn the run LED on or off to indicate the sensor is active or idle.
  bool setRunLedState(LedMode mode) {
    uint8_t payload[6] = {CMD_SET_LED_MODE, 0x00, 0x06, 0x00, uint8_t(mode), LED_KEEP};
    return sendCommand(payload, sizeof(payload), FRAME_TYPE_WRITE_REQUEST);
  }

  // Set the output LED state used by the sensor's reporting behavior.
  bool setOutLedState(LedMode mode) {
    uint8_t payload[6] = {CMD_SET_LED_MODE, 0x00, 0x06, 0x00, LED_KEEP, uint8_t(mode)};
    return sendCommand(payload, sizeof(payload), FRAME_TYPE_WRITE_REQUEST);
  }

  // Configure which output pin mode the C4002 uses for target events.
  bool setOutPinMode(OutPinMode mode) {
    uint8_t payload[5] = {CMD_CONFIG_OUT_MODE, 0x00, 0x05, 0x00, uint8_t(mode)};
    return sendCommand(payload, sizeof(payload), FRAME_TYPE_WRITE_REQUEST);
  }

  // Set the ambient-light threshold used by the C4002 for motion processing.
  bool setLightThresh(float thresholdLux) {
    const uint16_t threshold = uint16_t(thresholdLux * 10.0f);
    uint8_t payload[6] = {
      CMD_SET_LIGHT_THRESHOLD,
      0x00,
      0x06,
      0x00,
      uint8_t(threshold & 0xFF),
      uint8_t((threshold >> 8) & 0xFF),
    };
    return sendCommand(payload, sizeof(payload), FRAME_TYPE_WRITE_REQUEST);
  }

  // Set how long a target remains considered present after it disappears from the sensor field.
  bool setTargetDisappearDelay(uint16_t seconds) {
    uint8_t payload[6] = {
      CMD_TARGET_DISAPPEAR_DELAY,
      0x00,
      0x06,
      0x00,
      uint8_t(seconds & 0xFF),
      uint8_t((seconds >> 8) & 0xFF),
    };
    return sendCommand(payload, sizeof(payload), FRAME_TYPE_WRITE_REQUEST);
  }

  // Configure the distance gate array used for motion and presence detection windows.
  bool configureGate(uint8_t gateType, const uint8_t *gateData, size_t gateCount) {
    if (gateData == nullptr || gateCount == 0 || gateCount > 25) {
      return false;
    }

    uint8_t payload[32] = {0};
    const uint16_t dataLen = uint16_t(5 + gateCount);
    payload[0] = CMD_SET_DISTANCE_DOOR;
    payload[1] = 0x00;
    payload[2] = uint8_t(dataLen & 0xFF);
    payload[3] = uint8_t((dataLen >> 8) & 0xFF);
    payload[4] = gateType;

    for (size_t i = 0; i < gateCount; ++i) {
      payload[5 + i] = gateData[i];
    }

    return sendCommand(payload, dataLen, FRAME_TYPE_WRITE_REQUEST);
  }

  // Configure the target detection range for the C4002 in centimeters.
  bool setDetectRange(uint16_t closestCm, uint16_t farthestCm) {
    if (closestCm > farthestCm || farthestCm > 1100) {
      return false;
    }

    uint8_t payload[8] = {
      CMD_SET_DETECT_RANGE,
      0x00,
      0x08, 0x00,
      uint8_t(closestCm & 0xFF),
      uint8_t((closestCm >> 8) & 0xFF),
      uint8_t(farthestCm & 0xFF),
      uint8_t((farthestCm >> 8) & 0xFF)
    };
    return sendCommand(payload, sizeof(payload), FRAME_TYPE_WRITE_REQUEST);
  }

  // Wait for the next valid C4002 notification frame and decode it into a normalized sensor state.
  bool waitForNotification(C4002State &state, uint32_t timeoutMs) {
    const uint32_t startMs = millis();
    while ((millis() - startMs) < timeoutMs) {
      uint8_t packet[96] = {0};
      size_t actualLen = 0;
      if (readPacket(packet, sizeof(packet), actualLen, 30)) {
        diagnostics_.packets_read++;
        if (parseNotification(packet, actualLen, state)) {
          return true;
        }
      }
    }
    return false;
  }

  // Return the current packet and parsing diagnostics for debugging UART health.
  Diagnostics diagnostics() const {
    return diagnostics_;
  }

private:
  HardwareSerial *serial_ = nullptr;
  uint8_t rxPin_ = 18;
  uint8_t txPin_ = 17;
  uint32_t baudRate_ = 115200;
  Diagnostics diagnostics_ = {};

  // Build and send a framed write/read request to the C4002 and wait for the acknowledgement response.
  bool sendCommand(const uint8_t *payload, size_t payloadLen, uint8_t frameType) {
    if (serial_ == nullptr || payload == nullptr || payloadLen == 0) {
      return false;
    }

    uint8_t packet[32] = {0};
    const uint16_t totalLen = static_cast<uint16_t>(payloadLen + 10);
    packet[0] = FRAME_HEADER_1;
    packet[1] = FRAME_HEADER_2;
    packet[2] = FRAME_HEADER_3;
    packet[3] = FRAME_HEADER_4;
    packet[4] = uint8_t(totalLen & 0xFF);
    packet[5] = uint8_t((totalLen >> 8) & 0xFF);
    packet[6] = 0x00;
    packet[7] = frameType;

    for (size_t i = 0; i < payloadLen; ++i) {
      packet[8 + i] = payload[i];
    }

    const uint16_t sum = checksum(packet, 8 + payloadLen);
    packet[8 + payloadLen] = uint8_t(sum & 0xFF);
    packet[9 + payloadLen] = uint8_t((sum >> 8) & 0xFF);

    serial_->write(packet, totalLen);
    return waitForAck(200);
  }

  // Wait for the C4002 to acknowledge a write command before continuing.
  bool waitForAck(uint32_t timeoutMs) {
    const uint32_t startMs = millis();
    while ((millis() - startMs) < timeoutMs) {
      uint8_t packet[96] = {0};
      size_t actualLen = 0;
      if (!readPacket(packet, sizeof(packet), actualLen, 40)) {
        continue;
      }

      if (actualLen < 10) {
        continue;
      }

      // Notifications can arrive at any time; keep waiting for the write/read response frame.
      if (packet[7] == FRAME_TYPE_NOTIFICATION) {
        continue;
      }

      return packet[9] == SUCCEED;
    }

    return false;
  }

  // Read a raw UART packet from the sensor, skipping malformed frames until a valid one is found.
  bool readPacket(uint8_t *buffer, size_t bufferLen, size_t &actualLen, uint32_t timeoutMs) {
    if (buffer == nullptr || bufferLen == 0 || serial_ == nullptr) {
      return false;
    }

    const uint32_t startMs = millis();
    actualLen = 0;

    while ((millis() - startMs) < timeoutMs) {
      while (serial_->available() >= 8) {
        uint8_t header[8] = {0};
        for (size_t i = 0; i < 8; ++i) {
          int value = serial_->read();
          if (value < 0) {
            break;
          }
          header[i] = uint8_t(value);
        }

        if (header[0] != FRAME_HEADER_1 || header[1] != FRAME_HEADER_2 ||
            header[2] != FRAME_HEADER_3 || header[3] != FRAME_HEADER_4) {
          continue;
        }

        const uint16_t packetLen = uint16_t(header[4] | (header[5] << 8));
        if (packetLen < 10 || packetLen > bufferLen) {
          // Ignore malformed length and keep searching for a valid frame header.
          continue;
        }

        for (size_t i = 0; i < 8; ++i) {
          buffer[i] = header[i];
        }

        size_t remaining = packetLen - 8;
        size_t offset = 8;
        while (remaining > 0 && (millis() - startMs) < timeoutMs) {
          if (serial_->available() == 0) {
            delay(1);
            continue;
          }

          int value = serial_->read();
          if (value < 0) {
            continue;
          }

          buffer[offset++] = uint8_t(value);
          --remaining;
        }

        if (remaining == 0) {
          actualLen = packetLen;
          return true;
        }
      }
      delay(1);
    }

    return actualLen > 0;
  }

  // Decode a C4002 notification frame into motion, presence, distance, and energy values.
  bool parseNotification(const uint8_t *packet, size_t packetLen, C4002State &state) {
    if (packet == nullptr || packetLen < 12) {
      return false;
    }

    if (packet[0] != FRAME_HEADER_1 || packet[1] != FRAME_HEADER_2 ||
        packet[2] != FRAME_HEADER_3 || packet[3] != FRAME_HEADER_4) {
      return false;
    }

    const uint16_t declaredLen = uint16_t(packet[4] | (packet[5] << 8));
    if (declaredLen != packetLen) {
      return false;
    }

    const uint8_t msgType = packet[7];
    if (msgType != FRAME_TYPE_NOTIFICATION) {
      return false;
    }

    diagnostics_.notifications_seen++;

    const uint8_t noteCmd = packet[8];
    const uint8_t respCode = packet[9];
    const uint16_t dataLen = uint16_t(packet[10] | (packet[11] << 8));

    // C4002 notification payload uses a 4-byte data header (cmd, resp, len lo/hi).
    if (packetLen < (size_t(dataLen) + 10)) {
      diagnostics_.rejected_bad_len++;
      return false;
    }

    if (respCode != SUCCEED) {
      diagnostics_.rejected_bad_resp++;
      return false;
    }

    if (noteCmd == 0x03) {
      diagnostics_.calibration_notes_seen++;
      return false;
    }

    if (noteCmd != 0x60 || dataLen < 22) {
      diagnostics_.rejected_unknown_note++;
      return false;
    }

    const uint8_t *data = &packet[12];

    state.valid = true;
    state.note_type = noteCmd;
    state.target_state = data[0];
    state.distance_cm = uint16_t(data[9] | (data[10] << 8));
    state.energy = data[16];
    state.motion = (state.target_state == MOTION);
    state.presence = (state.target_state == PRESENCE || state.target_state == MOTION);
    diagnostics_.result_notes_parsed++;

    return true;
  }

  // Compute the 16-bit checksum used by the C4002 UART protocol.
  uint16_t checksum(const uint8_t *data, size_t len) const {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
      sum += data[i];
    }
    return uint16_t(sum & 0xFFFF);
  }
};

class Ens160Driver {
public:
  // Initialize the ENS160 sensor, verify the device ID, and enable the measurement mode.
  bool begin() {
    uint8_t partId[2] = {0};
    if (!readRegister(0x00, partId, sizeof(partId))) {
      return false;
    }

    if (!writeRegister(0x10, 0x02)) {
      return false;
    }

    delay(50);
    initialized_ = true;
    return true;
  }

  // Read the latest eCO2, TVOC, and AQI values from the ENS160 sensor.
  bool read(uint16_t &eco2, uint16_t &tvoc, uint8_t &aqi) {
    if (!initialized_) {
      return false;
    }

    uint8_t data[6] = {0};
    if (!readRegister(0x20, data, sizeof(data))) {
      return false;
    }

    aqi = data[1];
    tvoc = uint16_t(data[2] | (uint16_t(data[3]) << 8));
    eco2 = uint16_t(data[4] | (uint16_t(data[5]) << 8));
    return true;
  }

private:
  bool initialized_ = false;

  bool readRegister(uint8_t reg, uint8_t *data, size_t len) {
    if (data == nullptr || len == 0) {
      return false;
    }

    Wire.beginTransmission(0x53);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
      return false;
    }

    const uint8_t bytesRead = Wire.requestFrom(0x53, len);
    if (bytesRead != len) {
      return false;
    }

    for (uint8_t i = 0; i < len && Wire.available(); ++i) {
      data[i] = Wire.read();
    }

    return true;
  }

  // Write a single register value to the ENS160 sensor over I2C.
  bool writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(0x53);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission(true) == 0;
  }
};

class Bme280Driver {
public:
  // Initialize the BME280, verify the device ID, and configure the measurement mode.
  bool begin() {
    uint8_t chipId = 0;
    if (!readRegister(0xD0, &chipId, 1) || chipId != 0x60) {
      return false;
    }

    if (!readCalibration()) {
      return false;
    }

    if (!writeRegister(0xE0, 0xB6)) {
      return false;
    }

    delay(5);

    if (!writeRegister(0xF2, 0x01)) {
      return false;
    }

    if (!writeRegister(0xF5, 0x00)) {
      return false;
    }

    if (!writeRegister(0xF4, 0x27)) {
      return false;
    }

    initialized_ = true;
    return true;
  }

  // Read compensation-corrected temperature, humidity, and pressure values from the BME280.
  bool read(float &temperatureC, float &humidityRH, float &pressureHpa) {
    if (!initialized_) {
      return false;
    }

    uint8_t data[8] = {0};
    if (!readRegister(0xF7, data, sizeof(data))) {
      return false;
    }

    const int32_t rawPressure =
      (int32_t(data[0]) << 12) |
      (int32_t(data[1]) << 4) |
      (int32_t(data[2]) >> 4);
    const int32_t rawTemperature =
      (int32_t(data[3]) << 12) |
      (int32_t(data[4]) << 4) |
      (int32_t(data[5]) >> 4);
    const int32_t rawHumidity =
      (int32_t(data[6]) << 8) |
      int32_t(data[7]);

    compensate(rawTemperature, rawHumidity, rawPressure, temperatureC, humidityRH, pressureHpa);
    return true;
  }

private:
  struct CalibrationData {
    uint16_t dig_t1 = 0;
    int16_t dig_t2 = 0;
    int16_t dig_t3 = 0;
    uint16_t dig_p1 = 0;
    int16_t dig_p2 = 0;
    int16_t dig_p3 = 0;
    int16_t dig_p4 = 0;
    int16_t dig_p5 = 0;
    int16_t dig_p6 = 0;
    int16_t dig_p7 = 0;
    int16_t dig_p8 = 0;
    int16_t dig_p9 = 0;
    uint8_t dig_h1 = 0;
    int16_t dig_h2 = 0;
    uint8_t dig_h3 = 0;
    int16_t dig_h4 = 0;
    int16_t dig_h5 = 0;
    int8_t dig_h6 = 0;
  };

  bool readRegister(uint8_t reg, uint8_t *data, size_t len) {
    if (data == nullptr || len == 0) {
      return false;
    }

    Wire.beginTransmission(0x76);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
      return false;
    }

    const uint8_t bytesRead = Wire.requestFrom(0x76, len);
    if (bytesRead != len) {
      return false;
    }

    for (uint8_t i = 0; i < len && Wire.available(); ++i) {
      data[i] = Wire.read();
    }

    return true;
  }

  // Write a single register value into the BME280 configuration registers.
  bool writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(0x76);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission(true) == 0;
  }

  // Load the factory calibration coefficients needed to convert raw BME280 samples to real-world units.
  bool readCalibration() {
    uint8_t calib1[26] = {0};
    uint8_t humidity1 = 0;
    uint8_t calib2[7] = {0};

    if (!readRegister(0x88, calib1, sizeof(calib1))) {
      return false;
    }

    if (!readRegister(0xA1, &humidity1, 1)) {
      return false;
    }

    if (!readRegister(0xE1, calib2, sizeof(calib2))) {
      return false;
    }

    calibration_.dig_t1 = uint16_t(calib1[0] | (calib1[1] << 8));
    calibration_.dig_t2 = int16_t(calib1[2] | (calib1[3] << 8));
    calibration_.dig_t3 = int16_t(calib1[4] | (calib1[5] << 8));
    calibration_.dig_p1 = uint16_t(calib1[6] | (calib1[7] << 8));
    calibration_.dig_p2 = int16_t(calib1[8] | (calib1[9] << 8));
    calibration_.dig_p3 = int16_t(calib1[10] | (calib1[11] << 8));
    calibration_.dig_p4 = int16_t(calib1[12] | (calib1[13] << 8));
    calibration_.dig_p5 = int16_t(calib1[14] | (calib1[15] << 8));
    calibration_.dig_p6 = int16_t(calib1[16] | (calib1[17] << 8));
    calibration_.dig_p7 = int16_t(calib1[18] | (calib1[19] << 8));
    calibration_.dig_p8 = int16_t(calib1[20] | (calib1[21] << 8));
    calibration_.dig_p9 = int16_t(calib1[22] | (calib1[23] << 8));
    calibration_.dig_h1 = humidity1;
    calibration_.dig_h2 = int16_t(calib2[0] | (calib2[1] << 8));
    calibration_.dig_h3 = calib2[2];
    calibration_.dig_h4 = int16_t((calib2[3] << 4) | (calib2[4] & 0x0F));
    calibration_.dig_h5 = int16_t((calib2[5] << 4) | (calib2[4] >> 4));
    calibration_.dig_h6 = int8_t(calib2[6]);

    return true;
  }

  // Convert raw BME280 sensor data into calibrated temperature, humidity, and pressure readings.
  void compensate(int32_t rawTemperature, int32_t rawHumidity, int32_t rawPressure, float &temperatureC, float &humidityRH, float &pressureHpa) {
    const int32_t var1 = (((rawTemperature >> 3) - (int32_t(calibration_.dig_t1) << 1)) * int32_t(calibration_.dig_t2)) >> 11;
    const int32_t var2 = (((((rawTemperature >> 4) - int32_t(calibration_.dig_t1)) * ((rawTemperature >> 4) - int32_t(calibration_.dig_t1))) >> 12) * int32_t(calibration_.dig_t3)) >> 14;
    tFine_ = var1 + var2;

    const int32_t tempScaled = (tFine_ * 5 + 128) >> 8;
    temperatureC = float(tempScaled) / 100.0f;

    int64_t varP1 = int64_t(tFine_) - 128000;
    int64_t varP2 = varP1 * varP1 * int64_t(calibration_.dig_p6);
    varP2 += (varP1 * int64_t(calibration_.dig_p5)) << 17;
    varP2 += int64_t(calibration_.dig_p4) << 35;
    varP1 = ((varP1 * varP1 * int64_t(calibration_.dig_p3)) >> 8) + ((varP1 * int64_t(calibration_.dig_p2)) << 12);
    varP1 = (((int64_t(1) << 47) + varP1) * int64_t(calibration_.dig_p1)) >> 33;

    if (varP1 != 0) {
      int64_t pressure = 1048576 - rawPressure;
      pressure = (((pressure << 31) - varP2) * 3125) / varP1;
      varP1 = (int64_t(calibration_.dig_p9) * (pressure >> 13) * (pressure >> 13)) >> 25;
      varP2 = (int64_t(calibration_.dig_p8) * pressure) >> 19;
      pressure = ((pressure + varP1 + varP2) >> 8) + (int64_t(calibration_.dig_p7) << 4);
      pressureHpa = float(pressure) / 25600.0f;
    } else {
      pressureHpa = 0.0f;
    }

    int32_t varH = tFine_ - 76800;
    varH = (((((rawHumidity << 14) - (int32_t(calibration_.dig_h4) << 20) - (int32_t(calibration_.dig_h5) * varH)) + 16384) >> 15) *
            (((((((varH * int32_t(calibration_.dig_h6)) >> 10) * (((varH * int32_t(calibration_.dig_h3)) >> 11) + 32768)) >> 10) + 2097152) * int32_t(calibration_.dig_h2) + 8192) >> 14));
    varH = varH - (((((varH >> 15) * (varH >> 15)) >> 7) * int32_t(calibration_.dig_h1)) >> 4);
    varH = varH < 0 ? 0 : varH;
    varH = varH > 419430400 ? 419430400 : varH;
    humidityRH = float(varH >> 12) / 1024.0f;
  }

  bool initialized_ = false;
  CalibrationData calibration_ = {};
  int32_t tFine_ = 0;
};

class ContextEngine {
public:
  // Convert the full sensor snapshot into a room-level occupancy and air-quality context.
  void update(const SensorSnapshot &snapshot) {
    current_.occupied = snapshot.c4002_present;
    current_.moving = snapshot.motion_detected;
    current_.stationary = snapshot.static_presence;
    current_.temperature_c = snapshot.temperature_c;
    current_.humidity_rh = snapshot.humidity_rh;
    current_.eco2_ppm = snapshot.eco2_ppm;
    current_.tvoc_ppb = snapshot.tvoc_ppb;
    current_.air_quality_warning = snapshot.eco2_ppm > 1000.0f || snapshot.tvoc_ppb > 250.0f;
    current_.ventilation_recommended = current_.occupied && current_.air_quality_warning;
  }

  // Return the latest room context for the MQTT, HTTP, and serial reporting layers.
  RoomContext snapshot() const {
    return current_;
  }

private:
  RoomContext current_ = {};
};

C4002Driver c4002Driver;
Ens160Driver ens160Driver;
Bme280Driver bme280Driver;
ContextEngine contextEngine;
SensorSnapshot snapshot;
C4002State lastC4002State;
uint32_t lastC4002UpdateMs = 0;
uint32_t lastPublishMs = 0;
uint32_t sampleCount = 0;
WebServer mcpHttpServer(MCP_HTTP_PORT);
bool mcpHttpEnabled = false;
WiFiClient mqttNetClient;
PubSubClient mqttClient(mqttNetClient);
bool mqttEnabled = false;
uint32_t lastMqttReconnectAttemptMs = 0;
char mqttBrokerHost[128] = {0};
bool ntpTimeAvailable = false;
char ntpPrimaryServer[64] = {0};
char ntpSecondaryServer[64] = {0};
bool ntpSyncConfigured = false;
bool ntpSyncAttemptActive = false;
uint32_t ntpSyncStartMs = 0;
uint32_t ntpLastAttemptMs = 0;
char activeTimeZone[96] = "UTC0";
OccupancyHistoryEntry occupancyHistory[OCCUPANCY_HISTORY_DEPTH] = {};
size_t occupancyHistoryCount = 0;
size_t occupancyHistoryNext = 0;
bool occupancyEdgeStateInitialized = false;
bool lastRecordedOccupiedState = false;

// Apply configured timezone; fall back to UTC if missing or invalid.
void applyConfiguredTimeZone() {
  const char *const fallbackTz = "UTC0";
  const char *requestedTz = TIMEZONE;

  bool useFallback = (requestedTz == nullptr || requestedTz[0] == '\0');
  const char *candidateTz = useFallback ? fallbackTz : requestedTz;

  if (setenv("TZ", candidateTz, 1) != 0) {
    useFallback = true;
    candidateTz = fallbackTz;
    setenv("TZ", candidateTz, 1);
  }

  tzset();

  if (!useFallback && (tzname[0] == nullptr || tzname[0][0] == '\0')) {
    useFallback = true;
    candidateTz = fallbackTz;
    setenv("TZ", candidateTz, 1);
    tzset();
  }

  std::snprintf(activeTimeZone, sizeof(activeTimeZone), "%s", candidateTz);
  if (useFallback) {
    Serial.printf("Timezone fallback applied: %s\n", activeTimeZone);
  } else {
    Serial.printf("Timezone configured: %s\n", activeTimeZone);
  }
}

// Parse a comma-delimited NTP server list into primary and secondary hostnames.
void parseNtpServers(const char *csv, char *primary, size_t primaryLen, char *secondary, size_t secondaryLen) {
  if (primary == nullptr || primaryLen == 0 || secondary == nullptr || secondaryLen == 0) {
    return;
  }

  primary[0] = '\0';
  secondary[0] = '\0';

  if (csv == nullptr || csv[0] == '\0') {
    return;
  }

  auto copyTrimmedToken = [](const char *start, size_t len, char *dest, size_t destLen) {
    while (len > 0 && (*start == ' ' || *start == '\t')) {
      start++;
      len--;
    }
    while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t')) {
      len--;
    }
    if (destLen == 0) {
      return;
    }
    if (len >= destLen) {
      len = destLen - 1;
    }
    if (len > 0) {
      memcpy(dest, start, len);
    }
    dest[len] = '\0';
  };

  const char *comma = strchr(csv, ',');
  if (comma == nullptr) {
    copyTrimmedToken(csv, strlen(csv), primary, primaryLen);
    return;
  }

  copyTrimmedToken(csv, size_t(comma - csv), primary, primaryLen);
  copyTrimmedToken(comma + 1, strlen(comma + 1), secondary, secondaryLen);
}

// Return NTP time when available; otherwise keep legacy uptime-millis behavior.
uint64_t currentTimestampMs() {
  if (ntpTimeAvailable) {
    const time_t now = time(nullptr);
    if (now > 1000000000) {
      return uint64_t(now) * 1000ULL;
    }
    ntpTimeAvailable = false;
  }
  return uint64_t(millis());
}

// Start an asynchronous NTP synchronization attempt when networking is available.
void initializeNtpTime() {
  if (WiFi.status() != WL_CONNECTED) {
    ntpTimeAvailable = false;
    return;
  }

  if (!ntpSyncConfigured) {
    parseNtpServers(NTP_SERVERS, ntpPrimaryServer, sizeof(ntpPrimaryServer), ntpSecondaryServer, sizeof(ntpSecondaryServer));
    ntpSyncConfigured = true;
  }

  if (ntpPrimaryServer[0] == '\0') {
    Serial.println("NTP disabled: NTP_SERVERS is empty");
    ntpTimeAvailable = false;
    return;
  }

  if (ntpTimeAvailable || ntpSyncAttemptActive) {
    return;
  }

  const char *secondary = (ntpSecondaryServer[0] != '\0') ? ntpSecondaryServer : nullptr;
  configTime(0, 0, ntpPrimaryServer, secondary);
  applyConfiguredTimeZone();
  ntpSyncAttemptActive = true;
  ntpSyncStartMs = millis();
  ntpLastAttemptMs = ntpSyncStartMs;
  Serial.printf("NTP sync started using %s", ntpPrimaryServer);
  if (secondary != nullptr) {
    Serial.printf(", %s", secondary);
  }
  Serial.println();
}

// Check NTP sync status without blocking loop execution.
void pollNtpTime() {
  if (WiFi.status() != WL_CONNECTED) {
    ntpTimeAvailable = false;
    ntpSyncAttemptActive = false;
    return;
  }

  if (!ntpTimeAvailable && !ntpSyncAttemptActive) {
    const uint32_t nowMs = millis();
    if ((nowMs - ntpLastAttemptMs) >= 60000UL) {
      initializeNtpTime();
    }
    return;
  }

  if (!ntpSyncAttemptActive) {
    return;
  }

  const time_t now = time(nullptr);
  if (now > 1000000000) {
    ntpTimeAvailable = true;
    ntpSyncAttemptActive = false;
    Serial.println("NTP time synchronized; using internet time reference");
    return;
  }

  const uint32_t elapsedMs = millis() - ntpSyncStartMs;
  if (elapsedMs >= 10000UL) {
    ntpTimeAvailable = false;
    ntpSyncAttemptActive = false;
    Serial.println("NTP sync timeout; using uptime milliseconds for now");
  }
}

// Convert the MQTT client status code into a readable diagnostic label.
const char *mqttStateText(int state) {
  switch (state) {
    case -4:
      return "MQTT_CONNECTION_TIMEOUT";
    case -3:
      return "MQTT_CONNECTION_LOST";
    case -2:
      return "MQTT_CONNECT_FAILED";
    case -1:
      return "MQTT_DISCONNECTED";
    case 0:
      return "MQTT_CONNECTED";
    case 1:
      return "MQTT_BAD_PROTOCOL";
    case 2:
      return "MQTT_BAD_CLIENT_ID";
    case 3:
      return "MQTT_UNAVAILABLE";
    case 4:
      return "MQTT_BAD_CREDENTIALS";
    case 5:
      return "MQTT_UNAUTHORIZED";
    default:
      return "MQTT_UNKNOWN";
  }
}

// Strip any scheme prefix or trailing slash from the configured MQTT broker host.
void normalizeBrokerHost(const char *input, char *output, size_t outputLen) {
  if (output == nullptr || outputLen == 0) {
    return;
  }

  output[0] = '\0';
  if (input == nullptr || input[0] == '\0') {
    return;
  }

  const char *start = input;
  if (strncmp(start, "http://", 7) == 0) {
    start += 7;
  } else if (strncmp(start, "https://", 8) == 0) {
    start += 8;
  }

  size_t len = strnlen(start, outputLen - 1);
  while (len > 0 && (start[len - 1] == '/' || start[len - 1] == ' ')) {
    --len;
  }

  if (len >= outputLen) {
    len = outputLen - 1;
  }

  memcpy(output, start, len);
  output[len] = '\0';
}

// Escape JSON control characters so sensor data can be embedded inside MCP responses safely.
String jsonEscape(const String &in) {
  String out;
  out.reserve(in.length() + 32);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else {
      out += c;
    }
  }
  return out;
}

// Pull a JSON string field from a message body without needing a full parser.
String extractJsonStringField(const String &json, const char *field) {
  const String key = String("\"") + field + "\"";
  const int keyPos = json.indexOf(key);
  if (keyPos < 0) {
    return String();
  }

  int colonPos = json.indexOf(':', keyPos + key.length());
  if (colonPos < 0) {
    return String();
  }

  int firstQuote = json.indexOf('"', colonPos + 1);
  if (firstQuote < 0) {
    return String();
  }

  int secondQuote = json.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) {
    return String();
  }

  return json.substring(firstQuote + 1, secondQuote);
}

// Extract the JSON-RPC id field so responses can echo the caller's request id.
String extractJsonIdToken(const String &json) {
  const String key = "\"id\"";
  const int keyPos = json.indexOf(key);
  if (keyPos < 0) {
    return "null";
  }

  int colonPos = json.indexOf(':', keyPos + key.length());
  if (colonPos < 0) {
    return "null";
  }

  int valueStart = colonPos + 1;
  while (valueStart < (int)json.length() && isspace((unsigned char)json[valueStart])) {
    valueStart++;
  }
  if (valueStart >= (int)json.length()) {
    return "null";
  }

  if (json[valueStart] == '"') {
    int valueEnd = json.indexOf('"', valueStart + 1);
    if (valueEnd < 0) {
      return "null";
    }
    return json.substring(valueStart, valueEnd + 1);
  }

  int valueEnd = valueStart;
  while (valueEnd < (int)json.length() && json[valueEnd] != ',' && json[valueEnd] != '}') {
    valueEnd++;
  }
  return json.substring(valueStart, valueEnd);
}

// Build the latest room context payload used by MQTT, HTTP, and MCP responses.
String buildContextJson() {
  const RoomContext room = contextEngine.snapshot();
  const float distanceFt = snapshot.distance_m * 3.28084f;
  const float temperatureF = (snapshot.temperature_c * 9.0f / 5.0f) + 32.0f;
  const float pressureInHg = snapshot.pressure_hpa * 0.0295299830714f;
  const String timestampText = buildSampleTimeText(snapshot, ntpTimeAvailable);

  char buf[640] = {0};
  std::snprintf(
      buf,
      sizeof(buf),
      "{\"timestamp\":\"%s\",\"time_source_ntp\":%s,\"occupied\":%s,\"moving\":%s,\"stationary\":%s,\"ventilation_recommended\":%s,\"air_quality_warning\":%s,\"distance_ft\":%.2f,\"energy\":%d,\"eco2_ppm\":%.0f,\"tvoc_ppb\":%.0f,\"temperature_f\":%.2f,\"humidity_rh\":%.2f,\"pressure_inhg\":%.2f}",
      timestampText.c_str(),
      ntpTimeAvailable ? "true" : "false",
      room.occupied ? "true" : "false",
      room.moving ? "true" : "false",
      room.stationary ? "true" : "false",
      room.ventilation_recommended ? "true" : "false",
      room.air_quality_warning ? "true" : "false",
      distanceFt,
      snapshot.energy_level,
      snapshot.eco2_ppm,
      snapshot.tvoc_ppb,
      temperatureF,
      snapshot.humidity_rh,
      pressureInHg);
  return String(buf);
}

// Add one occupancy sample only when occupied flips, keeping only the latest depth entries.
void recordOccupancyHistory(const SensorSnapshot &sensor, const RoomContext &room) {
  if (!occupancyEdgeStateInitialized) {
    lastRecordedOccupiedState = room.occupied;
    occupancyEdgeStateInitialized = true;
    return;
  }

  if (room.occupied == lastRecordedOccupiedState) {
    return;
  }

  lastRecordedOccupiedState = room.occupied;

  OccupancyHistoryEntry &slot = occupancyHistory[occupancyHistoryNext];
  slot.timestamp_ms = sensor.timestamp_ms;
  slot.time_source_ntp = ntpTimeAvailable;
  slot.occupied = room.occupied;
  slot.moving = room.moving;
  slot.stationary = room.stationary;
  slot.distance_ft = sensor.distance_m * 3.28084f;
  slot.energy = sensor.energy_level;

  occupancyHistoryNext = (occupancyHistoryNext + 1) % OCCUPANCY_HISTORY_DEPTH;
  if (occupancyHistoryCount < OCCUPANCY_HISTORY_DEPTH) {
    occupancyHistoryCount++;
  }
}

// Build the occupancy history JSON from oldest to newest for API and MCP consumers.
String buildOccupancyHistoryJson() {
  String out;
  out.reserve(2200);
  out = String("{\"depth\":") + String(static_cast<unsigned>(OCCUPANCY_HISTORY_DEPTH)) +
        String(",\"count\":") + String(static_cast<unsigned>(occupancyHistoryCount)) +
        String(",\"entries\":[");

  const size_t start = (occupancyHistoryNext + OCCUPANCY_HISTORY_DEPTH - occupancyHistoryCount) % OCCUPANCY_HISTORY_DEPTH;
  for (size_t i = 0; i < occupancyHistoryCount; ++i) {
    const size_t idx = (start + i) % OCCUPANCY_HISTORY_DEPTH;
    const OccupancyHistoryEntry &entry = occupancyHistory[idx];
    SensorSnapshot entrySnapshot = {};
    entrySnapshot.timestamp_ms = entry.timestamp_ms;
    const String entryTimestampText = buildSampleTimeText(entrySnapshot, entry.time_source_ntp);

    char entryBuf[220] = {0};
    std::snprintf(
      entryBuf,
      sizeof(entryBuf),
      "{\"timestamp\":\"%s\",\"time_source_ntp\":%s,\"occupied\":%s,\"moving\":%s,\"stationary\":%s,\"distance_ft\":%.2f,\"energy\":%d}",
      entryTimestampText.c_str(),
      entry.time_source_ntp ? "true" : "false",
      entry.occupied ? "true" : "false",
      entry.moving ? "true" : "false",
      entry.stationary ? "true" : "false",
      entry.distance_ft,
      entry.energy);

    if (i > 0) {
      out += ',';
    }
    out += entryBuf;
  }

  out += "]}";
  return out;
}

// Format sample time for serial logs using NTP wall clock when synced, otherwise uptime milliseconds.
String buildSampleTimeText(const SensorSnapshot &sensor, bool useNtpTime) {
  if (useNtpTime) {
    const time_t sampleTime = static_cast<time_t>(sensor.timestamp_ms / 1000ULL);
    if (sampleTime > 1000000000) {
      struct tm localTime = {};
      localtime_r(&sampleTime, &localTime);
      char timeBuf[40] = {0};
      strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S%z", &localTime);
      return String(timeBuf);
    }
  }

  char uptimeBuf[32] = {0};
  std::snprintf(uptimeBuf,
                sizeof(uptimeBuf),
                "%llums",
                static_cast<unsigned long long>(sensor.timestamp_ms));
  return String(uptimeBuf);
}

// Send a structured JSON-RPC error payload for invalid MCP requests or tool calls.
void sendMcpError(const String &idToken, int code, const char *message) {
  String resp = String("{\"jsonrpc\":\"2.0\",\"id\":") + idToken +
                String(",\"error\":{\"code\":") + code +
                String(",\"message\":\"") + message + "\"}}";
  mcpHttpServer.send(200, "application/json", resp);
}

// Handle incoming MCP JSON-RPC requests for initialize, tools/list, resources/list, and data reads.
void handleMcpPost() {
  const String body = mcpHttpServer.arg("plain");
  const String idToken = extractJsonIdToken(body);
  const String method = extractJsonStringField(body, "method");

  if (method.length() == 0) {
    sendMcpError(idToken, -32600, "Invalid Request: missing method");
    return;
  }

  if (method == "initialize") {
    String resp = String("{\"jsonrpc\":\"2.0\",\"id\":") + idToken +
                  ",\"result\":{\"protocolVersion\":\"2024-11-05\",\"serverInfo\":{\"name\":\"room-ai-node-arduino\",\"version\":\"0.1.0\"},\"capabilities\":{\"tools\":{},\"resources\":{}}}}";
    mcpHttpServer.send(200, "application/json", resp);
    return;
  }

  if (method == "tools/list") {
    String resp = String("{\"jsonrpc\":\"2.0\",\"id\":") + idToken +
                  ",\"result\":{\"tools\":["
                  "{\"name\":\"get_area_data\",\"description\":\"Returns the current real-time environmental metrics (temperature, humidity, pressure, CO2, and TVOC levels) and human occupancy metrics (present, motion, static, distance, energy) for the area\",\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
                  "{\"name\":\"get_environmental_data\",\"description\":\"Returns temperature, humidity, and pressure metrics for the area\",\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
                  "{\"name\":\"get_occupancy_data\",\"description\":\"Returns human occupancy metrics (present, motion, static, distance, energy) for the area\",\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
                  "{\"name\":\"get_air_quality_data\",\"description\":\"Returns CO2 and TVOC levels for the area\",\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
                  "{\"name\":\"get_occupancy_history\",\"description\":\"Returns the most recent occupancy history entries (depth 10, oldest to newest)\",\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}}"
                  "]}}";
    mcpHttpServer.send(200, "application/json", resp);
    return;
  }

  if (method == "resources/list") {
    String resp = String("{\"jsonrpc\":\"2.0\",\"id\":") + idToken +
                  ",\"result\":{\"resources\":[{\"uri\":\"room://context/current\",\"name\":\"Current Room Context\",\"mimeType\":\"application/json\"},{\"uri\":\"room://occupancy/history\",\"name\":\"Occupancy History\",\"mimeType\":\"application/json\"}]}}";
    mcpHttpServer.send(200, "application/json", resp);
    return;
  }

  if (method == "resources/read") {
    const String resourceUri = extractJsonStringField(body, "uri");
    const bool readHistory = (resourceUri == "room://occupancy/history");
    const String payloadJson = readHistory ? buildOccupancyHistoryJson() : buildContextJson();
    const String responseUri = readHistory ? String("room://occupancy/history") : String("room://context/current");
    const String escaped = jsonEscape(payloadJson);
    String resp = String("{\"jsonrpc\":\"2.0\",\"id\":") + idToken +
                  ",\"result\":{\"contents\":[{\"uri\":\"" + responseUri + "\",\"mimeType\":\"application/json\",\"text\":\"" + escaped + "\"}]}}";
    mcpHttpServer.send(200, "application/json", resp);
    return;
  }

  if (method == "tools/call") {
    const String toolName = extractJsonStringField(body, "name");
    if (toolName != "get_area_data" && toolName != "get_environmental_data" && toolName != "get_occupancy_data" && toolName != "get_air_quality_data" && toolName != "get_occupancy_history") {
      sendMcpError(idToken, -32601, "Tool not found");
      return;
    }

    if (toolName == "get_environmental_data") {
      const float temperatureF = (snapshot.temperature_c * 9.0f / 5.0f) + 32.0f;
      const float pressureInHg = snapshot.pressure_hpa * 0.0295299830714f;
      char valueBuf[160] = {0};
      std::snprintf(valueBuf,
                    sizeof(valueBuf),
                    "{\"temperature_f\":%.2f,\"humidity_rh\":%.2f,\"pressure_inhg\":%.2f,\"sensor_ready\":%s}",
                    temperatureF,
                    snapshot.humidity_rh,
                    pressureInHg,
                    snapshot.bme280_ready ? "true" : "false");
      const String textPayload = jsonEscape(String(valueBuf));

      String resp = String("{\"jsonrpc\":\"2.0\",\"id\":") + idToken +
                    ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"" + textPayload + "\"}],\"structuredContent\":" +
                    String(valueBuf) +
                    ",\"isError\":false}}";
      mcpHttpServer.send(200, "application/json", resp);
      return;
    }

    if (toolName == "get_occupancy_data") {
      const float distanceFt = snapshot.distance_m * 3.28084f;
      char valueBuf[160] = {0};
      std::snprintf(valueBuf,
                    sizeof(valueBuf),
                    "{\"presence\":%s,\"motion\":%s,\"static\":%s,\"distance_ft\":%.2f,\"energy\":%d,\"sensor_ready\":%s}",
                    snapshot.c4002_present ? "true" : "false",
                    snapshot.motion_detected ? "true" : "false",
                    snapshot.static_presence ? "true" : "false",
                    distanceFt,
                    snapshot.energy_level,
                    lastC4002State.valid ? "true" : "false");
      const String textPayload = jsonEscape(String(valueBuf));

      String resp = String("{\"jsonrpc\":\"2.0\",\"id\":") + idToken +
                    ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"" + textPayload + "\"}],\"structuredContent\":" +
                    String(valueBuf) +
                    ",\"isError\":false}}";
      mcpHttpServer.send(200, "application/json", resp);
      return;
    }

    if (toolName == "get_air_quality_data") {
      char valueBuf[160] = {0};
      std::snprintf(valueBuf,
                    sizeof(valueBuf),
                    "{\"eco2_ppm\":%.0f,\"tvoc_ppb\":%.0f,\"sensor_ready\":%s}",
                    snapshot.eco2_ppm,
                    snapshot.tvoc_ppb,
                    snapshot.ens160_ready ? "true" : "false");
      const String textPayload = jsonEscape(String(valueBuf));

      String resp = String("{\"jsonrpc\":\"2.0\",\"id\":") + idToken +
                    ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"" + textPayload + "\"}],\"structuredContent\":" +
                    String(valueBuf) +
                    ",\"isError\":false}}";
      mcpHttpServer.send(200, "application/json", resp);
      return;
    }

    if (toolName == "get_occupancy_history") {
      const String historyJson = buildOccupancyHistoryJson();
      const String escaped = jsonEscape(historyJson);

      String resp = String("{\"jsonrpc\":\"2.0\",\"id\":") + idToken +
                    ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"" + escaped + "\"}],\"structuredContent\":" +
                    historyJson +
                    ",\"isError\":false}}";
      mcpHttpServer.send(200, "application/json", resp);
      return;
    }

    const String contextJson = buildContextJson();
    const String escaped = jsonEscape(contextJson);
    String resp = String("{\"jsonrpc\":\"2.0\",\"id\":") + idToken +
                  ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"" + escaped + "\"}],\"structuredContent\":" +
                  contextJson +
                  ",\"isError\":false,\"_meta\":{\"raw\":\"" + escaped + "\"}}}";
    mcpHttpServer.send(200, "application/json", resp);
    return;
  }

  if (method == "ping") {
    String resp = String("{\"jsonrpc\":\"2.0\",\"id\":") + idToken + ",\"result\":{}}";
    mcpHttpServer.send(200, "application/json", resp);
    return;
  }

  sendMcpError(idToken, -32601, "Method not found");
}

// Start the local MCP/HTTP server once Wi-Fi is connected and expose sensor data over HTTP.
void setupMcpHttpServer() {
  if (strlen(WIFI_SSID) == 0) {
    Serial.println("MCP HTTP disabled: WIFI_SSID is empty");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 10000) {
    delay(100);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("MCP HTTP disabled: WiFi connect timeout");
    return;
  }

  mcpHttpServer.on("/context", HTTP_METHOD_GET, []() {
    mcpHttpServer.send(200, "application/json", buildContextJson());
  });

  mcpHttpServer.on("/occupancy/history", HTTP_METHOD_GET, []() {
    mcpHttpServer.send(200, "application/json", buildOccupancyHistoryJson());
  });

  mcpHttpServer.on("/mcp", HTTP_METHOD_POST, handleMcpPost);

  mcpHttpServer.onNotFound([]() {
    mcpHttpServer.send(404, "application/json", "{\"error\":\"not found\"}");
  });

  mcpHttpServer.begin();
  mcpHttpEnabled = true;
  initializeNtpTime();
  Serial.printf("MCP HTTP ready: http://%s:%u/mcp\n", WiFi.localIP().toString().c_str(), MCP_HTTP_PORT);
}

// Configure the MQTT client only when a broker host and Wi-Fi link are available.
void setupMqttClient() {
  if (strlen(MQTT_BROKER_HOST) == 0) {
    Serial.println("MQTT disabled: MQTT_BROKER_HOST is empty");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("MQTT disabled: WiFi is not connected");
    return;
  }

  normalizeBrokerHost(MQTT_BROKER_HOST, mqttBrokerHost, sizeof(mqttBrokerHost));
  if (mqttBrokerHost[0] == '\0') {
    Serial.println("MQTT disabled: normalized MQTT_BROKER_HOST is empty");
    return;
  }

  mqttClient.setServer(mqttBrokerHost, MQTT_BROKER_PORT);
  mqttClient.setBufferSize(1024);
  mqttEnabled = true;
  Serial.printf("MQTT configured: %s:%u (state topic: %s)\n",
                mqttBrokerHost,
                MQTT_BROKER_PORT,
                MQTT_TOPIC_STATE);
}

// Maintain the MQTT connection state and reconnect with a throttled retry interval.
void ensureMqttConnected() {
  if (!mqttEnabled || mqttClient.connected()) {
    return;
  }

  const uint32_t nowMs = millis();
  if ((nowMs - lastMqttReconnectAttemptMs) < 5000UL) {
    return;
  }
  lastMqttReconnectAttemptMs = nowMs;

  String clientId = String("room-ai-node-") + String((uint32_t)ESP.getEfuseMac(), HEX);

  bool connected = false;
  if (strlen(MQTT_USERNAME) > 0) {
    connected = mqttClient.connect(
      clientId.c_str(),
      MQTT_USERNAME,
      MQTT_PASSWORD,
      MQTT_TOPIC_AVAILABILITY,
      0,
      true,
      "offline");
  } else {
    connected = mqttClient.connect(
      clientId.c_str(),
      MQTT_TOPIC_AVAILABILITY,
      0,
      true,
      "offline");
  }

  if (connected) {
    mqttClient.publish(MQTT_TOPIC_AVAILABILITY, "online", true);
    Serial.println("MQTT connected");
  } else {
    const int rc = mqttClient.state();
    Serial.printf("MQTT connect failed, rc=%d (%s)\n", rc, mqttStateText(rc));
  }
}

// Publish the most recent room context to the configured MQTT state topic.
void publishMqttState() {
  if (!mqttEnabled || !mqttClient.connected()) {
    return;
  }

  const String payload = buildContextJson();
  if (!mqttClient.publish(MQTT_TOPIC_STATE, payload.c_str(), false)) {
    Serial.println("MQTT publish failed");
  }
}

// Print a compact human-readable summary of the current sensor snapshot to the serial monitor.
void printSensorSnapshot(const SensorSnapshot &s) {
  const float distanceFt = s.distance_m * 3.28084f;
  const float temperatureF = (s.temperature_c * 9.0f / 5.0f) + 32.0f;
  const float pressureInHg = s.pressure_hpa * 0.0295299830714f;

  Serial.printf("C4002 present=%d motion=%d static=%d dist=%.2fft energy=%d\n",
                s.c4002_present,
                s.motion_detected,
                s.static_presence,
                distanceFt,
                s.energy_level);

  Serial.printf("ENS160 ready=%d eco2=%.0f tvoc=%.0f\n",
                s.ens160_ready,
                s.eco2_ppm,
                s.tvoc_ppb);

  Serial.printf("BME280 ready=%d temp=%.2fF humidity=%.2f%% pressure=%.2finHg\n",
                s.bme280_ready,
                temperatureF,
                s.humidity_rh,
                pressureInHg);

}

// Arduino setup routine: initialize hardware, configure sensor drivers, and start network interfaces.
void setup() {
  // Board startup sequence:
  // 1) open the serial console for logs,
  // 2) initialize I2C,
  // 3) configure the sensor drivers,
  // 4) start local HTTP/MQTT interfaces.
  // USB CDC on Boot must be enabled in the board menu for these logs to appear.
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("BOOT: setup() entered");

  const uint32_t serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart) < 3000) {
    delay(10);
  }

  Serial.println("BOOT: serial ready or wait timeout reached");
  applyConfiguredTimeZone();

  delay(500);

  Serial.println("BOOT: initializing I2C bus");
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(50);

  Serial.println("BOOT: initializing C4002");
  if (!c4002Driver.begin(Serial1, C4002_UART_RX, C4002_UART_TX, 115200)) {
    Serial.println("C4002 init failed");
  } else {
    const uint8_t gateState15[15] = {
      C4002Driver::SUCCEED, C4002Driver::SUCCEED, C4002Driver::SUCCEED, C4002Driver::SUCCEED, C4002Driver::SUCCEED,
      C4002Driver::SUCCEED, C4002Driver::SUCCEED, C4002Driver::SUCCEED, C4002Driver::SUCCEED, C4002Driver::SUCCEED,
      C4002Driver::SUCCEED, C4002Driver::SUCCEED, C4002Driver::SUCCEED, C4002Driver::SUCCEED, C4002Driver::SUCCEED,
    };

    if (!c4002Driver.setRunLedState(C4002Driver::LED_ON)) {
      Serial.println("C4002 run-led command failed");
    }
    if (!c4002Driver.setOutLedState(C4002Driver::LED_ON)) {
      Serial.println("C4002 out-led command failed");
    }
    if (!c4002Driver.setResolutionMode(C4002Driver::RESOLUTION_80CM)) {
      Serial.println("C4002 resolution command failed");
    }
    if (!c4002Driver.setOutPinMode(C4002Driver::OUT_PIN_MODE3)) {
      Serial.println("C4002 out-pin-mode command failed");
    }
    if (!c4002Driver.setLightThresh(0.0f)) {
      Serial.println("C4002 light-threshold command failed");
    }
    if (!c4002Driver.configureGate(0x00, gateState15, 15)) {
      Serial.println("C4002 motion gate command failed");
    }
    if (!c4002Driver.configureGate(0x01, gateState15, 15)) {
      Serial.println("C4002 presence gate command failed");
    }
    if (!c4002Driver.setTargetDisappearDelay(60)) {
      Serial.println("C4002 disappear-delay command failed");
    }
    if (!c4002Driver.setDetectRange(0, 1100)) {
      Serial.println("C4002 detect-range command failed");
    }
    // Match device report cadence to loop cadence to avoid UART backlog.
    if (!c4002Driver.setReportPeriod(50)) {
      Serial.println("C4002 report-period command failed");
    }
  }

  Serial.println("BOOT: initializing ENS160");
  ens160Driver.begin();
  Serial.println("BOOT: initializing BME280");
  bme280Driver.begin();

  Serial.println("BOOT: starting MCP HTTP");
  setupMcpHttpServer();
  Serial.println("BOOT: starting MQTT client");
  setupMqttClient();

  Serial.println("Room AI node starting");
  Serial.flush();
}

// Main execution loop: service HTTP requests, maintain sensor health, refresh readings, and publish state.
void loop() {
  pollNtpTime();

  // Handle any incoming MCP HTTP requests while the node is running.
  if (mcpHttpEnabled) {
    mcpHttpServer.handleClient();
  }

  // Maintain the MQTT connection and periodically publish the latest room state.

  ensureMqttConnected();
  if (mqttEnabled && mqttClient.connected()) {
    mqttClient.loop();
  }

  C4002State c4002State = {};
  if (c4002Driver.waitForNotification(c4002State, 120)) {
    if (C4002Driver::isZeroedState(c4002State)) {
      Serial.println("C4002 reported all-zero state; reinitializing UART and sensor configuration");
      if (c4002Driver.reinitialize()) {
        lastC4002State = {};
        lastC4002UpdateMs = 0;
      }
    } else {
      lastC4002State = c4002State;
      lastC4002UpdateMs = millis();
    }
  }

  const uint32_t nowMs = millis();
  if ((nowMs - lastPublishMs) < 5000UL) {
    return;
  }
  lastPublishMs = nowMs;

  uint16_t eco2 = 0;
  uint16_t tvoc = 0;
  uint8_t aqi = 0;
  float temperatureC = 0.0f;
  float humidityRH = 0.0f;
  float pressureHpa = 0.0f;
  const float tempOffsetC = TEMP_OFFSET_F * (5.0f / 9.0f);

  snapshot = SensorSnapshot{};
  snapshot.timestamp_ms = currentTimestampMs();

  const bool c4002StateFresh = (lastC4002State.valid && (nowMs - lastC4002UpdateMs) <= 90000UL);
  if (c4002StateFresh) {
    snapshot.c4002_present = lastC4002State.presence;
    snapshot.motion_detected = lastC4002State.motion;
    snapshot.static_presence = lastC4002State.presence && !lastC4002State.motion;
    snapshot.distance_m = float(lastC4002State.distance_cm) / 100.0f;
    snapshot.energy_level = int(lastC4002State.energy);
  } else if (lastC4002State.valid) {
    Serial.printf("C4002 stale for %lu ms; reinitializing\n",
                  static_cast<unsigned long>(nowMs - lastC4002UpdateMs));
    if (c4002Driver.reinitialize()) {
      lastC4002State = {};
      lastC4002UpdateMs = 0;
    }
  }

  if (ens160Driver.read(eco2, tvoc, aqi)) {
    snapshot.ens160_ready = true;
    snapshot.eco2_ppm = float(eco2);
    snapshot.tvoc_ppb = float(tvoc);
  }

  if (bme280Driver.read(temperatureC, humidityRH, pressureHpa)) {
    snapshot.bme280_ready = true;
    snapshot.temperature_c = temperatureC + tempOffsetC;
    snapshot.humidity_rh = humidityRH + HUMIDITY_OFFSET_RH;
    if (snapshot.humidity_rh < 0.0f) {
      snapshot.humidity_rh = 0.0f;
    } else if (snapshot.humidity_rh > 100.0f) {
      snapshot.humidity_rh = 100.0f;
    }
    snapshot.pressure_hpa = pressureHpa;
  }

  contextEngine.update(snapshot);
  const RoomContext room = contextEngine.snapshot();
  recordOccupancyHistory(snapshot, room);

  sampleCount++;
  const String sampleTimeText = buildSampleTimeText(snapshot, ntpTimeAvailable);
  Serial.printf("\n--- sample %lu t=%s (%s) ---\n",
                static_cast<unsigned long>(sampleCount),
                sampleTimeText.c_str(),
                ntpTimeAvailable ? "ntp" : "uptime");

  Serial.printf("occupied=%d moving=%d stationary=%d vent=%d\n",
                room.occupied,
                room.moving,
                room.stationary,
                room.ventilation_recommended);

  printSensorSnapshot(snapshot);
  publishMqttState();
}
