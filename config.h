#pragma once

#include <stdint.h>

// All network settings are kept in one place so each device can reuse the same
// code while only changing the local Wi-Fi and MQTT values in this file.
// Leave the strings empty to disable Wi-Fi/MQTT until they are configured.
static const char WIFI_SSID[] = "Kearson";
static const char WIFI_PASSWORD[] = "Kearson_AP";

// NTP server settings for time synchronization.
static const char NTP_SERVERS[] = "pool.ntp.org,time.nist.gov";

// Time zone for all NTP-based date/time strings.
// Use POSIX TZ format. Common examples:
// - UTC: UTC0
// - US Eastern: EST5EDT,M3.2.0/2,M11.1.0/2
// - US Central: CST6CDT,M3.2.0/2,M11.1.0/2
// - US Mountain: MST7MDT,M3.2.0/2,M11.1.0/2
// - US Pacific: PST8PDT,M3.2.0/2,M11.1.0/2
// - UK: GMT0BST,M3.5.0/1,M10.5.0/2
// - Central Europe: CET-1CEST,M3.5.0/2,M10.5.0/3
// If this is empty or invalid, UTC is used.
static const char TIMEZONE[] = "EST5EDT,M3.2.0/2,M11.1.0/2";

// Local HTTP/MCP endpoint for exposing room data to the host system.
constexpr uint16_t MCP_HTTP_PORT = 8081;

// MQTT broker settings used for publishing room-state updates.
static const char MQTT_BROKER_HOST[] = "192.168.1.47";
constexpr uint16_t MQTT_BROKER_PORT = 1883;
static const char MQTT_USERNAME[] = "mqtt-user";
static const char MQTT_PASSWORD[] = "mqttuser1!";

// Topic names for state and availability messages.
static const char MQTT_TOPIC_STATE[] = "room-ai-node/state";
static const char MQTT_TOPIC_AVAILABILITY[] = "room-ai-node/status";

// Occupancy transition history depth for HTTP/MCP responses.
// Must remain between 10 and 100 entries (validated in code).
constexpr uint16_t OCCUPANCY_HISTORY_DEPTH = 100;

// C4002 output pin mode: 1, 2, or 3.
// 1 = output HIGH only when motion is detected.
// 2 = output HIGH only when static presence is detected.
// 3 = output HIGH when either motion or presence is detected (recommended for
//    a room-occupied indicator and matches the project documentation/default).
// Keep this at 3 unless you intentionally want static-presence-only behavior.
constexpr uint8_t C4002_OUT_PIN_MODE = 3;
static_assert(C4002_OUT_PIN_MODE >= 1 && C4002_OUT_PIN_MODE <= 3,
              "C4002_OUT_PIN_MODE must be 1, 2, or 3");

// C4002 target detection range in centimeters.
// Valid range is 0-1100 cm, and the nearest value must be less than or equal to
// the farthest value. Use narrower values when the room is small or when you
// want to avoid detecting targets through adjacent walls or open hallways.
// For reference, 1 foot = 30.48 cm.
constexpr uint16_t C4002_DETECT_RANGE_NEAR_CM = 0;
constexpr uint16_t C4002_DETECT_RANGE_FAR_CM = 1100;
static_assert(C4002_DETECT_RANGE_NEAR_CM <= C4002_DETECT_RANGE_FAR_CM,
              "C4002_DETECT_RANGE_NEAR_CM must be <= C4002_DETECT_RANGE_FAR_CM");
static_assert(C4002_DETECT_RANGE_NEAR_CM <= 1100 && C4002_DETECT_RANGE_FAR_CM <= 1100,
              "C4002 detection range must be between 0 and 1100 cm");

// C4002 disappear-delay in seconds.
// 0 is the minimum value and disables extra hold time at the sensor level.
constexpr uint16_t C4002_TARGET_DISAPPEAR_DELAY_SECONDS = 0;

// Set to true to run the built-in C4002 environment calibration during boot.
// Set to false to skip calibration and use the sensor's default factory setup.
constexpr bool C4002_RUN_ENVIRONMENT_CALIBRATION = false;
// Set these to true/false to control each C4002 LED independently.
constexpr bool C4002_ENABLE_RUN_LED = true;
constexpr bool C4002_ENABLE_OUT_LED = true;

// Environmental sensor correction offsets.
// Positive values increase reported readings; negative values reduce them.
constexpr float TEMP_OFFSET_F = -5.1f;
constexpr float HUMIDITY_OFFSET_RH = 7.5f;