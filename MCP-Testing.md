# MCP Server Testing Guide (Arduino Sketch)

This guide tests the MCP HTTP endpoints implemented in AI_Occupancy_Sensor.ino.

## Prerequisites

- ESP32-C6 flashed with the current AI_Occupancy_Sensor.ino sketch.
- Serial Monitor open at 115200 baud.
- Your computer is on the same local network as the ESP32.
- Copy [config.h.example](config.h.example) to a local `config.h` and fill in your values before flashing.
- Wi‑Fi credentials, MQTT settings, and the MCP HTTP port are configured in [config.h](config.h). They should not be duplicated in the sketch itself.

## 1. Confirm MCP server startup

After boot, confirm a log line like:

```text
MCP HTTP ready: http://DEVICE_IP:8081/mcp
```

Use that DEVICE_IP in all commands below.

## 2. Quick endpoint check

Test direct context endpoint first:

```powershell
Invoke-RestMethod -Method Get -Uri "http://DEVICE_IP:8081/context"
```

Expected: JSON containing current room context fields such as occupied, moving, stationary, distance_ft, energy, temperature_f, and pressure_inhg.

## 3. MCP JSON-RPC tests

### Initialize

```powershell
Invoke-RestMethod -Method Post -Uri "http://DEVICE_IP:8081/mcp" -ContentType "application/json" -Body '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
```

Expected: result with protocolVersion, serverInfo, and capabilities.

### List tools

```powershell
Invoke-RestMethod -Method Post -Uri "http://DEVICE_IP:8081/mcp" -ContentType "application/json" -Body '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}'
```

Expected: includes tools named get_area_data, get_environmental_data, get_occupancy_data, and get_air_quality_data.

### Call tool: get_area_data

```powershell
Invoke-RestMethod -Method Post -Uri "http://DEVICE_IP:8081/mcp" -ContentType "application/json" -Body '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"get_area_data","arguments":{}}}'
```

Expected: result with both `content[0].text` and `structuredContent` containing live room context values.

### Call tool: get_environmental_data

```powershell
Invoke-RestMethod -Method Post -Uri "http://DEVICE_IP:8081/mcp" -ContentType "application/json" -Body '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"get_environmental_data","arguments":{}}}'
```

Expected: JSON with temperature_f, humidity_rh, pressure_inhg, and sensor_ready.

### Call tool: get_occupancy_data

```powershell
Invoke-RestMethod -Method Post -Uri "http://DEVICE_IP:8081/mcp" -ContentType "application/json" -Body '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"get_occupancy_data","arguments":{}}}'
```

Expected: JSON with presence, motion, static, distance_ft, energy, and sensor_ready.

### Call tool: get_air_quality_data

```powershell
Invoke-RestMethod -Method Post -Uri "http://DEVICE_IP:8081/mcp" -ContentType "application/json" -Body '{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"get_air_quality_data","arguments":{}}}'
```

Expected: JSON with eco2_ppm, tvoc_ppb, and sensor_ready.

### List resources

```powershell
Invoke-RestMethod -Method Post -Uri "http://DEVICE_IP:8081/mcp" -ContentType "application/json" -Body '{"jsonrpc":"2.0","id":7,"method":"resources/list","params":{}}'
```

Expected: includes uri room://context/current.

### Read resource

```powershell
Invoke-RestMethod -Method Post -Uri "http://DEVICE_IP:8081/mcp" -ContentType "application/json" -Body '{"jsonrpc":"2.0","id":8,"method":"resources/read","params":{"uri":"room://context/current"}}'
```

Expected: result.contents with current room context JSON text.

### Ping

```powershell
Invoke-RestMethod -Method Post -Uri "http://DEVICE_IP:8081/mcp" -ContentType "application/json" -Body '{"jsonrpc":"2.0","id":9,"method":"ping","params":{}}'
```

Expected: empty result object.

## 4. Optional curl equivalents

```bash
curl -s http://DEVICE_IP:8081/context
curl -s -X POST http://DEVICE_IP:8081/mcp -H "Content-Type: application/json" -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
```

## 5. Troubleshooting

- If you do not see "MCP HTTP ready" in serial logs:
  - Confirm the Wi‑Fi credentials in [config.h](config.h) are valid.
  - Check for "WiFi connect timeout" log.

- If /context fails:
  - Confirm computer and ESP32 are on the same LAN.
  - Confirm port 8081 is not blocked.

- If /context works but /mcp fails:
  - Verify request method is POST.
  - Verify Content-Type is application/json.
  - Verify JSON-RPC body includes method and id.

- If tools/call returns "Tool not found":
  - Use one of these names exactly: get_area_data, get_environmental_data, get_occupancy_data, get_air_quality_data.

## 6. Security note

The project keeps Wi‑Fi, MQTT, and MCP network settings in [config.h](config.h) instead of scattering them throughout the sketch. If this repository is shared, rotate credentials and keep the file out of source control or use a local-only override.

## 7. Tool rename note and alignment checklist

The MCP tool names changed to use a get_ prefix.

- area_data -> get_area_data
- environmental_data -> get_environmental_data
- occupancy_data -> get_occupancy_data
- air_quality_data -> get_air_quality_data

When MCP tool names or descriptions change, keep these files aligned:

- AI_Occupancy_Sensor.ino: tools/list entries and tools/call name routing.
- MCP-Test.ps1: tools/call request names.
- MCP-Testing.md: expected tool names and request examples.
- README.md: MCP tool contract summary.
- AI_Occupancy_Sensor.md: MCP tool architecture and examples.
