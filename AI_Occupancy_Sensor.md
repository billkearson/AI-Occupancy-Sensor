# AI Occupancy Sensor

## ESP32-C6 + C4002 + ENS160 + BME280

This document is the active architecture and reference document for the AI Occupancy Sensor project.

An AI-compatible intelligent room occupancy sensor in which the **FireBeetle 2 ESP32-C6 is the complete edge device**: sensor acquisition, context inference, MQTT integration, Home Assistant discovery, historical resources, and a minimal **MCP Streamable HTTP server** all run directly on the ESP32.

The design intentionally avoids a separate MCP gateway. Every MCP resource and tool is backed by data held by the ESP32.

## Technology stack

- Platform: FireBeetle 2 ESP32-C6
- Firmware: Arduino sketch written in C++
- Sensors: DFRobot C4002 mmWave, ENS160, BME280
- Connectivity: Wi‑Fi, MQTT, HTTP JSON endpoints, MCP Streamable HTTP
- Integration: Home Assistant and Open WebUI
- Enclosure: 3D-printed case using the STL files in the [case](case) folder

---

## 1. Project Goals

The system combines:

* **DFRobot C4002 mmWave Motion & Static Presence**
* **DFRobot ENS160 digital multi-gas sensor**
* **DFRobot BME280 temperature/humidity/pressure sensor**
* **DFRobot FireBeetle 2 ESP32-C6**

The finished node should answer questions such as:

> Is anyone in the room?

> How long has the room been occupied?

> Is the person moving or stationary?

> Is the environmental condition changing?

> Is air quality deteriorating while the room is occupied?

> What changed in the room during the last 30 minutes?

> Why did the system recommend ventilation?

The ESP32 performs deterministic edge inference and exposes the resulting state through:

1. **MQTT**
2. **Home Assistant MQTT Discovery**
3. **HTTP/JSON**
4. **MCP Streamable HTTP**

The architecture is deliberately **local-first**. No cloud service is required.

---

# 2. High-Level Architecture

```text
                         ROOM
                          │
          ┌───────────────┼────────────────┐
          │               │                │
          ▼               ▼                ▼
      ┌────────┐      ┌────────┐      ┌────────┐
      │ C4002  │      │ ENS160 │      │ BME280 │
      │ mmWave │      │ Air    │      │ Temp   │
      │        │      │ Quality│      │ RH     │
      │Presence│      │ TVOC   │      │Pressure│
      │Motion  │      │ eCO2   │      │        │
      └───┬────┘      └───┬────┘      └───┬────┘
          │ UART           │ I²C           │ I²C
          └────────────────┼────────────────┘
                           │
                           ▼
                ┌──────────────────────┐
                │    ESP32-C6          │
                │                      │
                │ Sensor Manager       │
                │ Feature Engine       │
                │ Context Engine       │
                │ Event Engine         │
                │ History Buffer       │
                │ MQTT Client          │
                │ HTTP Server          │
                │ MCP Server            │
                └───────┬───────┬──────┘
                        │       │
                  MQTT  │       │ HTTP
                        │       │
                ┌───────▼───┐   │
                │   Home    │   │
                │ Assistant │   │
                └───────────┘   │
                                │
                         ┌──────▼───────┐
                         │ MCP Client   │
                         │ / AI Agent   │
                         └──────────────┘
```

The important architectural constraint is:

> **The ESP32 is the source of truth.**

Home Assistant and MCP clients consume the ESP32's state. They do not own the room's resources or intelligence.

---

# 3. Hardware

## 3.1 FireBeetle 2 ESP32-C6

The ESP32-C6 is the central controller.

The ESP32-C6 provides a 32-bit RISC-V processor, Wi-Fi 6, Bluetooth LE, and 802.15.4 connectivity. The current project is implemented as a single Arduino sketch running directly on the board, with the same device responsibilities handled in-code rather than by a separate ESP-IDF app structure.

---

## 3.2 C4002

The C4002 provides the occupancy-related information:

* static presence
* motion
* distance
* target information
* target energy/intensity
* direction where supported by the configured C4002 operating mode

The C4002 communicates with the ESP32 over UART.

### Power note for the C4002

The C4002 needed more power than the ESP32 could reliably provide through its GPIO pin supply path. In the physical build, the sensor is powered from the ESP32 board's USB 5V rail using a visible red tap wire, as seen in the back-case image. This red wire is a deliberate power feed from the board's USB connection and is not a GPIO-supplied power path.

This hardware arrangement was necessary to keep the C4002 stable while the ESP32 continued to act as the local controller, MQTT client, and MCP endpoint host.

---

## 3.3 ENS160

The ENS160 provides:

* TVOC
* eCO₂
* air-quality information

The ENS160 should be treated as an **air-quality/context sensor**, not as a precision CO₂ sensor.

The firmware and API should therefore use the field name:

```text
eco2_ppm
```

rather than:

```text
co2_ppm
```

---

## 3.4 BME280

The BME280 provides:

* temperature
* relative humidity
* atmospheric pressure

The BME280 and ENS160 share the I²C bus.

---

## 3.5 Case Build and Open WebUI Validation

The project now includes real build photos, printable enclosure STL files in the `case/` folder, and an Open WebUI screenshot under `images/`:

### Case hardware fasteners

The 3D-printed enclosure uses the following mounting hardware:

- Sensor mounting screws: M2 x 6 mm
- Sensor mounting screws: M2 x 8 mm
- Case assembly hex head bolts: M2 x 12 mm
- Case assembly threaded inserts: M2 x 4 x 3.5 mm

The M2 x 6 mm and M2 x 8 mm screws were used to attach the sensors to the case. The M2 x 12 mm hex head bolts and M2 x 4 x 3.5 mm threaded inserts were used to secure the case panels together.

### 3D printable case files

- [case/Case3_front.stl](case/Case3_front.stl)
- [case/Case3_middle.stl](case/Case3_middle.stl)
- [case/Case3_back.stl](case/Case3_back.stl)

### Case photos

Front view:

![Case front view](images/front.jpg)

Front view (alternate angle):

![Alternate front view](images/front2.jpg)

Back view:

![Case back view](images/back.jpg)

Back view (alternate angle):

![Alternate back view](images/back2.jpg)

Case parts and internals:

![Case parts and internals](images/case_parts.jpg)

### Open WebUI integration screenshot

This screenshot shows the sensor exposed through MCP and consumed in Open WebUI:

![Open WebUI with sensor data](images/open_webui.jpg)

### Home Assistant integration screenshot

This screenshot shows the sensor data exposed in Home Assistant:

![Home Assistant sensor view](images/Home%20Assistant.jpg)

---

## Quick Start

1. Install the ESP32 Arduino board package and choose the FireBeetle 2 ESP32-C6 profile.
2. Copy [config.h.example](config.h.example) to a local [config.h](config.h) and populate Wi‑Fi, MQTT, and MCP settings.
3. Compile and upload [AI_Occupancy_Sensor.ino](AI_Occupancy_Sensor.ino) to the board.
4. Open the serial monitor at 115200 baud to confirm startup logs and sensor initialization.
5. Verify the local HTTP and MCP endpoints on the board's LAN address.
6. Run [MCP-Test.ps1](MCP-Test.ps1) or the examples in [MCP-Testing.md](MCP-Testing.md) to validate MCP behavior.

## Troubleshooting

- The board does not show startup logs: confirm the ESP32-C6 profile and serial monitor settings.
- The C4002 is unstable or silent: verify UART wiring and check the USB 5V power tap used for the sensor.
- MQTT or Wi‑Fi does not connect: review the shared configuration in [config.h](config.h).
- A request to `/context` or `/mcp` fails: confirm the endpoint is being called with the correct HTTP method and JSON-RPC payload.
- Values seem incorrect: check calibration and threshold settings in [config.h](config.h) before testing again.

---

# 4. Proposed Wiring

Use the FireBeetle 2 ESP32-C6 I²C pins:

```text
ESP32-C6
GPIO19 / SDA ────────── ENS160 SDA
                  └──── BME280 SDA

GPIO20 / SCL ────────── ENS160 SCL
                  └──── BME280 SCL

3V3 ─────────────────── ENS160 VCC
  └──────────────────── BME280 VCC

GND ─────────────────── ENS160 GND
  └──────────────────── BME280 GND
```

DFRobot documents GPIO19 as SDA and GPIO20 as SCL on the FireBeetle 2 ESP32-C6. Verify the exact board revision before final assembly. ([Espressif Systems][1])

Use a hardware UART for the C4002:

```text
ESP32-C6 UART TX ───── C4002 RX
ESP32-C6 UART RX ───── C4002 TX
GND ────────────────── C4002 GND
```

The exact UART GPIO assignment should remain a board configuration value rather than being scattered throughout the firmware.

Current default mapping: ESP32-C6 UART TX = GPIO 16 and ESP32-C6 UART RX = GPIO 17.

Network settings are centralized in the shared [config.h](config.h) file. This includes Wi‑Fi, MQTT broker details, and the MCP HTTP port so the project has a single source of truth instead of duplicate values in multiple files.

## Acknowledgement

Special thanks to DFRobot for providing the hardware used in this project as part of their C4002 Free Trial Program, in Kit 3: Multi-Sensor Fusion Kit (Enabling richer environmental sensing).

Included hardware:

- FireBeetle 2 ESP32-C6 x 1 (DFR1075)
- Fermion C4002 mmWave Human Presence Sensor x 1 (SEN0691)
- Fermion ENS160 + BME280 Environmental Sensor x 1 (SEN0335)

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for the full text.

The repository also includes [config.h.example](config.h.example), which is the template to copy locally as `config.h` and populate with your environment-specific values before building or flashing.

Verify the C4002 module's power and logic-level requirements before connecting it directly to 3.3 V GPIO.

---

# 5. Software Architecture

The primary runtime is the single-file Arduino sketch:

```text
AI_Occupancy_Sensor.ino
├── Configuration include
│   └── config.h
├── Sensor drivers (in-sketch classes)
│   ├── C4002Driver (UART occupancy/presence)
│   ├── Ens160Driver (I2C air quality)
│   └── Bme280Driver (I2C temperature/humidity/pressure)
├── Data models
│   ├── SensorSnapshot
│   └── RoomContext
├── Context inference
│   └── ContextEngine update and state tracking
├── Integrations
│   ├── MQTT publish and discovery payloads
│   ├── HTTP endpoints
│   └── MCP Streamable HTTP resources/tools
└── Main loop orchestration
  ├── Sensor polling
  ├── Context update
  ├── Telemetry/log output
  └── MCP/HTTP request handling
```

The current repository is intentionally Arduino-first and does not keep a separate ESP-IDF app tree as the active code path. The architecture remains modular in design, but the actual runtime is the single-file sketch that directly owns the sensor drivers, inference engine, and network integrations.

---

# 6. Runtime Tasks

Use FreeRTOS tasks to keep sensor acquisition independent from network activity.

```text
                    ESP32-C6
                       │
       ┌───────────────┼────────────────┐
       │               │                │
       ▼               ▼                ▼
 C4002 Task       Environment Task   Network Task
       │               │                │
       ▼               ▼                │
 Radar State       Env State             │
       │               │                │
       └───────┬───────┘                │
               ▼                        │
        Feature Engine                  │
               │                        │
               ▼                        │
        Context Engine                  │
               │                        │
               ▼                        │
          Event Engine ─────────────────┤
               │                        │
        ┌──────┴──────┐                 │
        ▼             ▼                 ▼
      MQTT          History          HTTP/MCP
```

Recommended starting rates:

| Function            |         Rate |
| ------------------- | -----------: |
| C4002 acquisition   |      5–10 Hz |
| BME280              |         1 Hz |
| ENS160              |         1 Hz |
| Feature calculation |         1 Hz |
| Context engine      |         1 Hz |
| MQTT telemetry      |  1–5 seconds |
| MQTT state          |    On change |
| MCP state           |   On request |
| MCP events          | Event-driven |

Current Arduino sketch behavior (`AI_Occupancy_Sensor.ino`) differs slightly for debug visibility:

- C4002 UART notifications are polled continuously.
- Combined telemetry is printed every 5 seconds.
- Console display uses feet (`ft`), Fahrenheit (`F`), and inches of mercury (`inHg`).

---

# 7. Canonical Room Observation

All sensor libraries should feed a normalized internal structure.

```cpp
struct RoomObservation {
    uint64_t timestamp_ms;

    // Environment
    float temperature_c;
    float humidity_pct;
    float pressure_hpa;

    // Air quality
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
    uint8_t aqi;

    // Radar
    bool presence;
    bool motion;
    float target_distance_m;
    uint8_t target_energy;
    float target_speed;
    int8_t target_direction;
    uint8_t target_count;

    // Radar/environment information
    float light_level;

    // Derived values
    float activity_score;
    float presence_confidence;
    float temperature_trend;
    float humidity_trend;
    float eco2_trend;
    float tvoc_trend;
};
```

Everything else should derive from this structure.

---

# 8. Feature Engine

Raw sensor values are converted into temporal features.

```text
presence_duration
absence_duration
motion_rate
stationary_duration

temperature_delta_5m
humidity_delta_5m

eco2_delta_5m
tvoc_delta_5m

activity_score
presence_confidence

air_quality_trend
environmental_anomaly_score
```

The system should maintain short rolling windows rather than storing every raw sample indefinitely.

Suggested windows:

```text
10 seconds
1 minute
5 minutes
15 minutes
1 hour
24 hours
```

---

# 9. Context Engine

The first version should use a deterministic state machine.

Do **not** make an LLM responsible for basic occupancy inference.

```text
                 ┌─────────────┐
                 │    EMPTY    │
                 └──────┬──────┘
                        │
                    presence
                        │
                        ▼
                 ┌─────────────┐
                 │  OCCUPIED   │
                 └──────┬──────┘
                        │
             ┌──────────┼──────────┐
             │          │          │
             ▼          ▼          ▼
          ACTIVE     STATIONARY  TRANSITION
             │          │
             │          ▼
             │     LONG_STATIONARY
             │          │
             └────┬─────┘
                  ▼
            ENVIRONMENTAL
               CONTEXT
```

Initial states:

```text
EMPTY
OCCUPIED
ACTIVE
STATIONARY
LONG_STATIONARY
AIR_DEGRADING
HOT
HUMID
VENTILATION_RECOMMENDED
ENVIRONMENTAL_ANOMALY
```

---

# 10. Confidence Rather Than Absolute Claims

The system should never pretend that the sensors know more than they actually do.

Use:

```json
{
  "state": "stationary",
  "confidence": 0.91
}
```

rather than:

```json
{
  "person_is_working": true
}
```

Higher-level interpretations can be represented as hypotheses:

```json
{
  "context": "work_like_stationary_activity",
  "confidence": 0.78
}
```

The distinction is important because mmWave presence plus environmental measurements cannot reliably determine a person's exact activity.

---

# 11. Canonical Room State

The ESP32 maintains one current room state.

```json
{
  "schema": "roomai.v1",

  "device": {
    "id": "roomai-01",
    "room": "office",
    "firmware": "0.1.0"
  },

  "presence": {
    "present": true,
    "confidence": 0.97,
    "duration_s": 4230,
    "motion": false,
    "distance_m": 2.14
  },

  "activity": {
    "state": "stationary",
    "score": 0.17,
    "confidence": 0.88
  },

  "environment": {
    "temperature_c": 24.3,
    "humidity_pct": 48.2,
    "pressure_hpa": 1014.1
  },

  "air": {
    "eco2_ppm": 812,
    "tvoc_ppb": 193,
    "aqi": 1
  },

  "context": {
    "room_state": "occupied",
    "air_state": "good",
    "ventilation_recommended": false
  }
}
```

This structure is the foundation for MQTT, Home Assistant, HTTP, and MCP.

---

# 12. Local History

Because **all resources must live on the ESP32**, the device needs a local history mechanism.

Do not attempt to retain every high-frequency sensor reading forever.

Instead, maintain:

### Short-term raw history

```text
10 Hz radar-derived observations
approximately 1–5 minutes
```

### 1-second/1-minute aggregates

```text
temperature
humidity
pressure
TVOC
eCO₂
presence
motion
activity
```

### Event history

Store important events for a much longer period:

```text
presence_started
presence_ended
activity_changed
air_quality_degrading
ventilation_recommended
environmental_anomaly
temperature_threshold
humidity_threshold
sensor_error
```

Use a circular buffer.

For longer persistence, periodically write compact aggregate/event records to NVS or a dedicated flash partition.

Avoid writing every sensor sample to flash because flash endurance is finite.

---

# 13. MQTT Architecture

MQTT remains the Home Assistant integration layer.

Use:

```text
roomai/<room>/
```

Example:

```text
roomai/office/state
roomai/office/telemetry
roomai/office/event
roomai/office/availability

roomai/office/command/recalibrate
roomai/office/command/reset
roomai/office/command/set_config
```

---

# 14. MQTT State

Example:

```json
{
  "state": "stationary",
  "presence": true,
  "presence_confidence": 0.96,
  "activity": "low",
  "activity_confidence": 0.87,
  "air_quality": "good",
  "ventilation_recommended": false
}
```

Publish this when the state changes.

---

# 15. MQTT Telemetry

Example:

```json
{
  "ts": 1786560000,
  "temperature_c": 24.3,
  "humidity_pct": 47.8,
  "pressure_hpa": 1014.2,
  "tvoc_ppb": 182,
  "eco2_ppm": 684,
  "aqi": 1,
  "presence": true,
  "motion": false,
  "distance_m": 2.14,
  "activity_score": 0.17
}
```

Telemetry can be published periodically.

---

# 16. MQTT Events

Example:

```json
{
  "event": "ventilation_recommended",
  "severity": "warning",
  "confidence": 0.89,
  "duration_occupied_s": 5400,
  "eco2_trend_ppm_min": 4.7,
  "tvoc_trend_ppb_min": 1.8
}
```

Events should be published immediately.

---

# 17. Home Assistant

Use MQTT Discovery so the ESP32 presents itself as a single device containing multiple entities.

Expose:

### Binary sensors

```text
Room Presence
Room Motion
Ventilation Recommended
Environmental Anomaly
```

### Sensors

```text
Temperature
Humidity
Pressure
eCO₂
TVOC
AQI
Target Distance
Activity Score
Presence Confidence
```

### Diagnostic entities

```text
Wi-Fi RSSI
MQTT Connected
Firmware Version
Uptime
Free Heap
Sensor Status
```

### State entities

```text
Room State
Activity State
Air Quality State
```

Home Assistant's MQTT integration supports device discovery and multiple MQTT components grouped under a device. ([Espressif Systems][2])

---

# 18. ESP32 HTTP API

The ESP32 should expose a small normal HTTP API in addition to MCP.

```text
GET /api/v1/status
GET /api/v1/state
GET /api/v1/telemetry
GET /api/v1/events
GET /api/v1/history
GET /api/v1/health

POST /api/v1/command/recalibrate
POST /api/v1/command/reset
```

This API is useful for:

* debugging
* firmware development
* Home Assistant integrations
* curl
* browser testing
* MCP implementation
* diagnostics

ESP-IDF provides a lightweight HTTP server with URI handlers and persistent connection support. ([Espressif Systems][3])

---

# 19. MCP Design

## Principle

The MCP implementation should be **minimal**.

The ESP32 does not need a general-purpose MCP framework.

Implement only the MCP protocol features required by this application:

```text
initialize
tools/list
tools/call
resources/list
resources/read
```

Optionally support:

```text
notifications
```

for event streaming.

Do not implement:

```text
prompts
sampling
elicitation
roots
tasks
```

unless a later requirement makes them necessary.

---

# 20. MCP Transport

Use **Streamable HTTP**.

The MCP endpoint is:

```text
POST /mcp
GET  /mcp
```

The current MCP transport specification defines Streamable HTTP around a single MCP endpoint supporting POST and GET, with server-to-client streaming available through SSE. ([Espressif Systems][2])

The implementation should use:

```text
Content-Type: application/json
```

for ordinary JSON responses and:

```text
Content-Type: text/event-stream
```

when an MCP response needs to stream.

Do not implement the older standalone HTTP+SSE transport.

---

# 21. Minimal MCP JSON-RPC Layer

MCP messages are JSON-RPC 2.0.

The minimum request structure is:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/list",
  "params": {}
}
```

Response:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "tools": []
  }
}
```

The ESP32 implementation should use a small JSON parser and avoid dynamic allocation where possible.

---

# 22. MCP Initialization

The server should respond to:

```text
initialize
```

with its supported protocol version and capabilities.

Conceptually:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "protocolVersion": "<supported-version>",
    "capabilities": {
      "resources": {},
      "tools": {}
    },
    "serverInfo": {
      "name": "roomai-esp32",
      "version": "0.1.0"
    }
  }
}
```

The exact protocol version should be a firmware configuration constant and updated as the MCP specification evolves.

---

# 23. Active MCP Tools

The current Arduino implementation exposes these MCP tools:

* `get_area_data`
* `get_environmental_data`
* `get_occupancy_data`
* `get_air_quality_data`

These tool names are the canonical names used by the sketch, the PowerShell validation script, and the markdown test guide.

The active implementation in AI_Occupancy_Sensor.ino returns structured JSON payloads and a text payload for each tool call, and it validates the tool name against the current list before executing.

---

# 24. Active MCP Resource

The current implementation exposes the following MCP resource URI:

```text
room://context/current
```

This resource returns the latest room context in JSON form and is used by the live MCP validation flow.

---
# 30. MCP Streaming

Streaming should be minimal and event-driven.

A client can issue:

```text
GET /mcp
```

with an MCP-compatible request for an SSE stream.

The ESP32 maintains a small list of connected stream clients.

When a significant room event occurs:

```text
presence_started
presence_ended
activity_changed
air_quality_degrading
ventilation_recommended
environmental_anomaly
sensor_error
```

the event is serialized as an MCP notification.

Conceptually:

```text
ESP32
  │
  ├── Context Engine
  │       │
  │       ▼
  │    Event Queue
  │       │
  │       ▼
  │    MCP Stream
  │       │
  │       ▼
  │    AI Client
```

Do not stream every BME280 or ENS160 reading.

Stream **meaningful state changes**.

---

# 31. MCP Session Management

For the minimum implementation:

* support one or a small configurable number of concurrent MCP sessions
* assign a session identifier when required by the negotiated transport
* bound session lifetime
* clean up disconnected clients
* keep per-session memory small

Do not allocate a large object graph for each connection.

A fixed session pool is preferable:

```cpp
struct McpSession {
    bool active;
    uint32_t id;
    int socket;
    uint32_t last_activity;
};
```

For a constrained embedded implementation, something like:

```text
MAX_MCP_SESSIONS = 2
```

is a reasonable starting point.

Important note: the current Arduino implementation does not yet enforce an active session pool or track a real MCP session count at runtime; this value is the intended design limit, not a currently enforced runtime cap.

---

# 32. HTTP Server Resource Limits

The ESP-IDF HTTP server is designed as a lightweight embedded server and supports URI handlers and configurable server resources. ([Espressif Systems][3])

Configure conservatively.

Example starting point:

```text
max URI handlers:       small
max open sockets:       4–6
MCP sessions:           2
HTTP request size:      bounded
JSON payload size:      bounded
history response:       bounded
```

The exact values should be tuned after measuring heap usage on the actual firmware.

---

# 33. Memory Strategy

The ESP32-C6 has limited embedded memory, so avoid unnecessary dynamic allocation.

Prefer:

```text
static buffers
fixed-size arrays
ring buffers
bounded JSON
fixed MCP sessions
fixed event queue
```

Avoid:

```text
unbounded history
large JSON documents
full MQTT payload retention
large HTTP request bodies
unbounded SSE connections
```

The goal is for the MCP server to be just another small HTTP handler, not a large framework.

---

# 34. Data Flow

## Sensor → Context

```text
C4002
   ↓
Radar observation

ENS160
   ↓
Air observation

BME280
   ↓
Environment observation

         ↓

Normalized RoomObservation

         ↓

Feature Engine

         ↓

Context Engine

         ↓

RoomState + Events
```

## RoomState → MQTT

```text
RoomState
    ↓
MQTT publisher
    ↓
Home Assistant
```

## RoomState → MCP

```text
RoomState
    ↓
MCP resource handler
    ↓
JSON-RPC response
    ↓
MCP client / AI
```

## Event → MCP stream

```text
Context Engine
      ↓
Event
      ↓
Event Queue
      ↓
MCP Stream
      ↓
AI client
```

---

# 35. Example AI Interaction

An MCP client asks:

> What's happening in the office?

The client can request:

```text
room://current
room://environment
room://events
```

The ESP32 returns:

```text
Presence: true
Presence confidence: 97%
Duration: 71 minutes
Activity: stationary
Temperature: 74°F
Humidity: 48%
eCO₂: 812 ppm
TVOC: 193 ppb

Recent event:
air_quality_degrading
```

The AI can then reason:

> The room is occupied and the occupant has been relatively stationary for about an hour. Temperature and humidity are stable, while eCO₂ has been trending upward. The system has therefore flagged ventilation as potentially useful.

The AI isn't responsible for measuring or storing the room.

It is simply reasoning over the ESP32's local resources.

---

# 36. Example "Why?" Interaction

MCP tool:

```text
get_events
```

and:

```text
get_history
```

could return:

```text
Presence:
71 minutes

eCO₂:
620 → 812 ppm

TVOC:
121 → 193 ppb

Temperature:
23.7 → 24.3 °C

Humidity:
46 → 48 %
```

The AI can infer:

```text
Occupied
+
eCO₂ rising
+
TVOC rising
+
long occupancy
=
ventilation recommendation
```

The important point is that the **evidence originates on the ESP32**.

---

# 37. MQTT and MCP Are Independent

The system should continue operating if either integration disappears.

### MQTT unavailable

The ESP32 continues:

```text
sensor acquisition
context inference
local history
HTTP
MCP
```

### MCP client unavailable

The ESP32 continues:

```text
sensor acquisition
context inference
MQTT
Home Assistant
local history
```

### Home Assistant unavailable

The ESP32 continues to function independently.

This makes the room node autonomous.

---

# 38. Local-First Failure Model

```text
                    ESP32-C6
                       │
          ┌────────────┼─────────────┐
          │            │             │
       Sensors       History      Context
          │            │             │
          └────────────┼─────────────┘
                       │
                ┌──────┴───────┐
                │              │
              MQTT            MCP
                │              │
          optional          optional
```

No cloud dependency exists in the core architecture.

---

# 39. Security

Do not expose the ESP32 MCP endpoint directly to the public Internet.

Recommended:

```text
Internet
    X
    │
    │ no port forwarding
    ▼
Local LAN
    │
    ├── Home Assistant
    ├── MQTT broker
    └── MCP client
           │
           ▼
       ESP32-C6
```

For local deployment, authenticate the MCP endpoint and validate the expected host/origin behavior appropriate to the deployment.

The MCP specification calls out DNS-rebinding protection and recommends validating the `Origin` header and implementing authentication for Streamable HTTP servers. ([Espressif Systems][2])

At minimum:

```text
LAN-only
+
authentication
+
bounded connections
+
no Internet exposure
```

For a higher-security installation, use HTTPS/TLS as supported by ESP-IDF and provision device certificates.

---

# 40. OTA

Firmware should eventually support OTA.

The firmware update should replace:

```text
sensor drivers
context engine
MQTT
HTTP
MCP
```

as one atomic firmware image.

Do not make the MCP protocol implementation independently updatable.

---

# 41. Configuration

Store configuration in NVS.

Example:

```json
{
  "device_id": "roomai-01",
  "room": "office",

  "wifi": {
    "ssid": "...",
    "password": "..."
  },

  "mqtt": {
    "host": "192.168.1.10",
    "port": 1883,
    "username": "...",
    "password": "..."
  },

  "context": {
    "absence_timeout_s": 300,
    "stationary_timeout_s": 600,
    "ventilation_delay_s": 300
  },

  "mcp": {
    "enabled": true,
    "max_sessions": 2
  }
}
```

Credentials should not be exposed through the public MCP resources.

`room://config` should return only safe configuration:

```json
{
  "room": "office",
  "absence_timeout_s": 300,
  "stationary_timeout_s": 600,
  "mcp_enabled": true
}
```

Never return Wi-Fi or MQTT passwords.

---

# 42. Implementation Notes

The current codebase is intentionally organized as a single Arduino sketch rather than a separate ESP-IDF application tree. The design still preserves clear boundaries for sensor drivers, state inference, and integration logic, but these are implemented directly in the sketch instead of being split into an external ESP-IDF component layout.

The DFRobot sensor libraries can still be wrapped behind the project's own sensor interfaces for future refactoring or porting work.

---

# 43. Recommended Interfaces

Keep the application independent of specific sensor libraries.

```cpp
class RadarSensor {
public:
    virtual bool begin() = 0;
    virtual bool update(RadarObservation& observation) = 0;
};

class AirSensor {
public:
    virtual bool begin() = 0;
    virtual bool update(AirObservation& observation) = 0;
};

class EnvironmentSensor {
public:
    virtual bool begin() = 0;
    virtual bool update(EnvironmentObservation& observation) = 0;
};
```

Then:

```text
DFRobot C4002
      ↓
RadarSensor

DFRobot ENS160
      ↓
AirSensor

DFRobot BME280
      ↓
EnvironmentSensor
```

This prevents the rest of the application from becoming coupled to vendor APIs.

---

# 44. Context Engine API

The context engine can have a simple interface:

```cpp
class ContextEngine {
public:
    void update(const RoomObservation& observation);

    const RoomState& state() const;

    bool hasNewEvent() const;

    bool popEvent(RoomEvent& event);
};
```

The MQTT and MCP layers both consume the resulting `RoomState`.

---

# 45. MCP Server API

The embedded MCP implementation can remain similarly small:

```cpp
class McpServer {
public:
    void begin(httpd_handle_t server);

    esp_err_t handlePost(httpd_req_t* request);
    esp_err_t handleGet(httpd_req_t* request);

private:
    void handleInitialize(...);
    void handleToolsList(...);
    void handleToolsCall(...);
    void handleResourcesList(...);
    void handleResourcesRead(...);
};
```

The server should call application functions rather than directly accessing sensor drivers.

For example:

```text
MCP
 │
 ▼
RoomState
 │
 ├── ContextEngine
 ├── History
 └── EventQueue
```

not:

```text
MCP
 │
 ├── C4002
 ├── ENS160
 └── BME280
```

MCP should consume **room information**, not raw hardware interfaces.

---

# 46. Minimum MCP Request Routing

The implementation can use a simple dispatcher:

```text
POST /mcp
     │
     ▼
Parse JSON-RPC
     │
     ├── initialize
     │
     ├── tools/list
     │
     ├── tools/call
     │
     ├── resources/list
     │
     └── resources/read
```

Unknown methods return the appropriate JSON-RPC/MCP error.

This keeps the embedded implementation small.

---

# 47. Active MCP Tool Routing

```text
tools/call
     │
     ├── get_area_data
     │        ↓
     │    current room context JSON
     │
     ├── get_environmental_data
     │        ↓
     │    temperature / humidity / pressure
     │
     ├── get_occupancy_data
     │        ↓
     │    presence / motion / static / distance / energy
     │
     └── get_air_quality_data
              ↓
          eCO₂ / TVOC
```

This matches the tool names currently implemented in AI_Occupancy_Sensor.ino.

---

# 48. Active Resource Routing

```text
resources/read
       │
       └── room://context/current
               ↓
          current room context JSON
```

This is the live MCP resource implemented by the current sketch.

---

# 49. What the ESP32 Should NOT Do

The ESP32 should not attempt to:

* run an LLM
* maintain an unlimited database
* store years of raw telemetry
* proxy arbitrary Internet requests
* implement every MCP capability
* expose arbitrary filesystem access
* execute arbitrary code requested through MCP
* let an AI directly manipulate GPIO without explicit application-level authorization

The ESP32 should remain a **bounded, deterministic room intelligence appliance**.

---

# 50. What the AI Should Do

The AI/client should handle:

* natural-language questions
* correlation between resources
* explanations
* summarization
* higher-level reasoning
* natural-language recommendations
* cross-room reasoning if multiple nodes are available

Example:

```text
ESP32:
"eCO₂ increased 31% during occupancy."

AI:
"The room has been occupied for approximately two hours and
eCO₂ has steadily increased. Ventilation may be beneficial."
```

---

# 51. Multiple Rooms

The architecture scales naturally.

Each room gets its own ESP32:

```text
roomai-office
roomai-bedroom
roomai-kitchen
roomai-living
```

Each has:

```text
own sensors
own state
own history
own MQTT topics
own MCP endpoint
```

An MCP client can connect to each device separately.

Alternatively, a future local MCP aggregator could be added without changing the individual room nodes.

The important point is that **the room node itself remains complete**.

---

# 52. Example Final Deployment

```text
                       HOME LAN
                           │
       ┌───────────────────┼───────────────────┐
       │                   │                   │
       ▼                   ▼                   ▼
 ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
 │ Office ESP  │     │ Bedroom ESP │     │ Kitchen ESP │
 │             │     │             │     │             │
 │ C4002       │     │ C4002       │     │ C4002       │
 │ ENS160      │     │ ENS160      │     │ ENS160      │
 │ BME280      │     │ BME280      │     │ BME280      │
 │             │     │             │     │             │
 │ MQTT        │     │ MQTT        │     │ MQTT        │
 │ MCP/HTTP    │     │ MCP/HTTP    │     │ MCP/HTTP    │
 └──────┬──────┘     └──────┬──────┘     └──────┬──────┘
        │                   │                   │
        └───────────────────┼───────────────────┘
                            │
                     ┌──────▼──────┐
                     │    MQTT     │
                     │   Broker    │
                     └──────┬──────┘
                            │
                     ┌──────▼──────┐
                     │    Home     │
                     │  Assistant  │
                     └─────────────┘

AI Client
    │
    ├──── MCP → Office ESP
    ├──── MCP → Bedroom ESP
    └──── MCP → Kitchen ESP
```

---
sensor_error
```

Everything else can wait.

---

# 54. Final Architecture

The resulting device is essentially a self-contained **AI-readable room sensor**:

```text
┌──────────────────────────────────────────────────────┐
│                    ESP32-C6 ROOM AI                  │
│                                                      │
│  ┌────────┐  ┌────────┐  ┌────────┐                 │
│  │ C4002  │  │ ENS160 │  │ BME280 │                 │
│  └───┬────┘  └───┬────┘  └───┬────┘                 │
│      │            │            │                     │
│      └────────────┼────────────┘                     │
│                   ▼                                  │
│          ┌─────────────────┐                         │
│          │ Sensor Manager  │                         │
│          └────────┬────────┘                         │
│                   ▼                                  │
│          ┌─────────────────┐                         │
│          │ Feature Engine  │                         │
│          └────────┬────────┘                         │
│                   ▼                                  │
│          ┌─────────────────┐                         │
│          │ Context Engine  │                         │
│          └────────┬────────┘                         │
│                   ▼                                  │
│          ┌─────────────────┐                         │
│          │ Room State      │                         │
│          │ Event History   │                         │
│          │ Local History   │                         │
│          └───────┬─┬───────┘                         │
│                  │ │                                 │
│          ┌───────┘ └────────┐                        │
│          ▼                  ▼                        │
│       MQTT                MCP/HTTP                   │
│          │                  │                        │
└──────────┼──────────────────┼────────────────────────┘
           │                  │
           ▼                  ▼
     Home Assistant       AI / MCP Client
```

The central design rule is:

> **Sense locally. Infer locally. Store locally. Expose locally.**

MQTT makes the node useful to Home Assistant.

The minimal Streamable HTTP MCP server makes the same node directly understandable to an AI client.

And because the **resources, history, events, context state, and sensor data all live on the ESP32**, the MCP interface remains a thin protocol layer over the device's existing room-intelligence model rather than introducing another backend.

### References

* [MCP specification — Streamable HTTP transport] [MCP Streamable HTTP specification](https://modelcontextprotocol.io/specification/2025-06-18/basic/transports?utm_source=chatgpt.com)
* [ESP-IDF ESP32-C6 documentation] [ESP32-C6 ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/?utm_source=chatgpt.com)
* [ESP-IDF HTTP Server] [ESP-IDF HTTP Server documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/protocols/esp_http_server.html?utm_source=chatgpt.com)
* [Home Assistant MQTT integration] [Home Assistant MQTT documentation](https://www.home-assistant.io/integrations/mqtt/?utm_source=chatgpt.com)

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/index.html?utm_source=chatgpt.com "Get Started - ESP32-C6 - — ESP-IDF Programming Guide v6.0.2 documentation"
[2]: https://docs.espressif.com/projects/esp-idf/en/v3.3.3/api-reference/protocols/esp_http_server.html?utm_source=chatgpt.com "HTTP Server — ESP-IDF Programming Guide v3.3.3 documentation"
[3]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/protocols/index.html?utm_source=chatgpt.com "Application Protocols - ESP32-C6 - — ESP-IDF Programming Guide v6.0.2 documentation"
