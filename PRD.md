# Product Requirements Document: Smart Meat Thermometer

**Version:** 1.1  
**Date:** 2026-04-11  
**Status:** Draft

---

## 1. Overview

### 1.1 Purpose

This document defines the requirements for a smart meat thermometer system built on the Nordic Semiconductor nRF7002 DK development board. The device monitors up to four temperature sensors simultaneously during a grilling session, transmits readings to a Home Assistant instance via MQTT over WiFi, and alerts the user via LED when target temperatures are reached.

### 1.2 Goals

- Provide real-time, multi-probe temperature monitoring during grilling sessions.
- Integrate seamlessly with Home Assistant for data visualization and setpoint control.
- Offer a simple physical UI via one active button and two LEDs.
- Support zero-configuration WiFi provisioning via BLE.

### 1.3 Out of Scope

- Mobile application development.
- Cloud connectivity (beyond local MQTT broker).
- Battery management / low-power optimization (initial version targets mains/USB power).

---

## 2. Hardware Platform

| Component | Details |
|---|---|
| Development Board | Nordic Semiconductor nRF7002 DK |
| Wireless | WiFi 6 (802.11ax) via nRF7002 + BLE 5.x via nRF5340 |
| Buttons | 2× onboard push buttons (Button 1 used; Button 2 reserved / unused) |
| LEDs | 2× onboard LEDs (LED 1, LED 2) |
| Sensor Interface | Up to 4 temperature sensors (see Section 3) |

### 2.1 Button Assignments

| Button | Function |
|---|---|
| Button 1 | **Start / Stop measurement cycle** — toggles between IDLE and MEASURING mode |
| Button 2 | Reserved — not used in v1.0 |

### 2.2 LED Assignments

| LED | Function |
|---|---|
| LED 1 | **Connectivity indicator** — ON steadily when WiFi/MQTT is connected; blinks on error |
| LED 2 | **Session indicator** — ON when MEASURING; flashes when any sensor reaches its target temperature |

---

## 3. Temperature Sensor Interface

### 3.1 Maximum Sensor Count

The system supports **up to 4 temperature sensors** connected simultaneously. Sensors are detected dynamically at session start.

### 3.2 Sensor Type Recommendation

The sensor type is not yet decided. The following comparison is provided to inform the selection:

| Type | Temp Range | Accuracy | Driver Complexity | Cost |
|---|---|---|---|---|
| **NTC Thermistor** | −55 °C to +150 °C | ±0.5–1 °C | Low (ADC + lookup table) | Very low (< €1) |
| **Thermocouple (K-type)** | −200 °C to +1260 °C | ±1–2 °C | Medium (requires amplifier IC, e.g. MAX31855; SPI driver) | Low–Medium (€2–5 for IC) |
| **PT100 / PT1000 RTD** | −200 °C to +850 °C | ±0.1–0.3 °C | High (requires precision current source or dedicated IC, e.g. MAX31865) | Medium–High |

**Recommendation for this use case:** An **NTC Thermistor** (e.g. 100 kΩ, food-grade probe) is the most pragmatic choice. Grilling temperatures fall well within its range (0–150 °C), driver implementation is minimal (single ADC channel per sensor + Steinhart-Hart equation), and cost is negligible. A **K-type thermocouple with MAX31855** is the recommended fallback if temperatures above 150 °C (e.g. searing) are needed, at the cost of a SPI-based driver per sensor.

### 3.3 Sensor Discovery

- The firmware shall enumerate connected sensors at power-on and at the start of each grilling session.
- Each detected sensor is assigned a stable index (1–4) based on its physical connection port.
- If user wants to start a session, but no sensors are connected, an error shall be logged and sent over MQTT
- Sensors connected or disconnected mid-session shall be handled gracefully (log an error, continue with remaining sensors).

---

## 4. Connectivity

### 4.1 WiFi Provisioning via BLE

- On first boot (or when no WiFi credentials are stored), the device shall advertise a BLE GATT service for provisioning.
- The user connects to the device via BLE (e.g. using the Nordic nRF Connect app or a custom companion app) and submits SSID and password.
- Credentials are persisted in non-volatile storage (NVS/flash).
- On subsequent boots, the device shall connect to WiFi automatically using stored credentials.
- If WiFi connection fails after 3 retries, LED 1 shall blink rapidly to indicate a connectivity error.

### 4.2 MQTT

| Parameter | Value |
|---|---|
| Protocol | MQTT 3.1.1 |
| Broker | User-configured (local, e.g. Mosquitto running alongside Home Assistant) |
| Authentication | Username / password (configurable via BLE provisioning or NVS) |
| QoS | QoS 1 (at least once) for temperature readings and alerts |

#### 4.2.1 MQTT Topic Structure

```
meatometer/
├── sensor/<id>/temperature        # Published by device — current reading (°C)
├── sensor/<id>/target             # Subscribed by device — target setpoint (°C)
├── sensor/<id>/target_reached     # Published by device — boolean alert
├── session/state                  # Published by device — "active" | "idle" | "error"
└── device/status                  # Published by device — "online" | "offline" (LWT)
```

`<id>` is an integer 1–4 corresponding to the sensor index.

#### 4.2.2 MQTT Payload Format

All payloads are plain UTF-8 strings or JSON as noted:

| Topic | Direction | Payload Example |
|---|---|---|
| `sensor/<id>/temperature` | Device → Broker | `72.4` |
| `sensor/<id>/target` | Broker → Device | `75.0` |
| `sensor/<id>/target_reached` | Device → Broker | `true` |
| `session/state` | Device → Broker | `active` |
| `device/status` | Device → Broker | `online` |

#### 4.2.3 Publishing Interval

Temperature readings are published every **5 seconds** per active sensor during a grilling session.

#### 4.2.4 Last Will and Testament (LWT)

The device shall register an LWT message on connect:
- **Topic:** `meatometer/device/status`
- **Payload:** `offline`
- **Retain:** true

On clean disconnect (power-off), the device shall publish `online` → `offline` before disconnecting.

---

## 5. Home Assistant Integration

### 5.1 MQTT Auto-Discovery

The device shall publish Home Assistant MQTT auto-discovery configuration messages on boot so that sensors and controls appear automatically in Home Assistant without manual YAML configuration.

Discovery topics follow the Home Assistant convention:

```
homeassistant/sensor/meatometer_<id>/config
homeassistant/number/meatometer_target_<id>/config
```

Each sensor entity shall include: `name`, `state_topic`, `unit_of_measurement: "°C"`, `device_class: temperature`, and `unique_id`.

Each target number entity shall include: `name`, `command_topic`, `min: 0`, `max: 150`, `step: 0.5`, `unit_of_measurement: "°C"`.

### 5.2 Displaying Temperature Readings

- Each active sensor appears as a separate **sensor entity** in Home Assistant.
- Values update every 5 seconds.
- Inactive (not connected) sensor slots shall not publish and may be hidden via `availability_topic`.

### 5.3 Setting Target Temperatures

- Each sensor has a corresponding **Number entity** in Home Assistant acting as the target setpoint.
- When the user changes a setpoint in Home Assistant, the value is published to `meatometer/sensor/<id>/target`.
- The device subscribes to these topics and stores received values in flash (NVS) so they persist across power cycles.
- If no target has been set for a sensor, no target-reached alert logic is active for that sensor.

---

## 6. Device State Machine

The device operates as a finite state machine (FSM) with four states. The firmware is always running from the moment the board is powered on — there is no software power-on/off via button.

### 6.1 State Diagram

```
                        ┌─────────────────────────────────┐
          Board powered │                                 │
         ───────────────▶   BOOTING                       │
                        │   • Init peripherals            │
                        │   • Connect WiFi / MQTT         │
                        │   • Publish HA discovery        │
                        │   • Subscribe to target topics  │
                        └──────────────┬──────────────────┘
                                       │ WiFi + MQTT connected
                                       │ (or no credentials → BLE
                                       │  provisioning first)
                                       ▼
                        ┌─────────────────────────────────┐
          ┌─────────────▶   IDLE                           │◀──────────────┐
          │             │   • LED 1 ON (steady)            │               │
          │             │   • LED 2 OFF                    │               │
          │             │   • MQTT subscriptions active    │               │
          │             │   • No sensor polling            │               │
          │             └──────────────┬──────────────────┘               │
          │                            │ Button 1 pressed                  │
          │                            ▼                                   │
          │             ┌─────────────────────────────────┐               │
          │             │   MEASURING                      │               │
          │             │   • LED 1 ON (steady)            │               │
          │             │   • LED 2 ON (steady)            │               │
          │             │   • Sensors enumerated           │               │
          │             │   • Poll + publish every 5 s     │               │
          │             │   • Monitor vs. target setpoints │               │
          └─────────────┤                                  │               │
   Button 1 pressed     └──────────────┬──────────────────┘               │
   (from ALERT state                   │ Any sensor reaches target         │
    → also returns                     ▼                                   │
    to IDLE)           ┌─────────────────────────────────┐                │
                        │   ALERT (sub-state of MEASURING) │               │
                        │   • LED 1 ON (steady)            │               │
                        │   • LED 2 flashing (500/500 ms)  │               │
                        │   • Polling + publishing continue│               │
                        │   • target_reached published     │───────────────┘
                        └─────────────────────────────────┘
                                    Button 1 pressed

         ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─
           CONNECTIVITY LOST (any state except BOOTING):                 │
         │   • LED 1 blinks slowly (1 s ON / 1 s OFF)
           • Readings buffered locally                                    │
         │   • Auto-reconnect with exponential back-off
           • On reconnect: flush buffer, LED 1 returns to steady ON       │
          ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
```

### 6.2 State Definitions

#### BOOTING

- **Entry:** Board is powered on.
- **Actions:**
  - Initialise all peripherals (ADC, GPIO, BLE, WiFi).
  - If no WiFi credentials are stored: start BLE provisioning advertisement and wait for credentials before proceeding.
  - Connect to WiFi and MQTT broker.
  - Publish Home Assistant auto-discovery messages.
  - Subscribe to all `sensor/<id>/target` topics.
  - Load persisted target temperatures from NVS.
- **Exit condition:** WiFi and MQTT are connected → transition to **IDLE**.
- **LED state:** LED 1 blinks rapidly during boot/connection; LED 2 OFF.

#### IDLE

- **Entry:** Boot complete, or Button 1 pressed while in MEASURING or ALERT.
- **Actions:**
  - Maintain active MQTT connection (receive target updates, respond to broker keep-alive).
  - No sensor polling occurs.
  - Publish `session/state = idle`.
- **Exit condition:** Button 1 pressed → transition to **MEASURING**.
- **LED state:** LED 1 ON (steady); LED 2 OFF.

#### MEASURING

- **Entry:** Button 1 pressed while in IDLE.
- **Actions:**
  - Enumerate connected sensors; assign indices 1–4.
  - Publish `session/state = active`.
  - Begin polling all sensors every 5 seconds.
  - Publish each reading to `sensor/<id>/temperature`.
  - After each reading cycle, compare each sensor value against its stored target. If any sensor meets or exceeds its target → transition to **ALERT**.
- **Exit condition:** Button 1 pressed → transition to **IDLE**.
- **LED state:** LED 1 ON (steady); LED 2 ON (steady).

#### ALERT *(sub-state of MEASURING)*

- **Entry:** One or more sensors reach or exceed their target temperature.
- **Actions:**
  - Publish `true` to `sensor/<id>/target_reached` for each sensor that triggered.
  - Continue all MEASURING actions (polling, publishing) unchanged.
  - Flash LED 2 at 500 ms ON / 500 ms OFF.
  - The alert is **not cleared** if the temperature subsequently drops below the target.
  - Additional sensors reaching their target do not change the LED pattern (already flashing).
- **Exit condition:** Button 1 pressed → transition to **IDLE** (alert clears).
- **LED state:** LED 1 ON (steady); LED 2 flashing.

### 6.3 State Transition Table

| Current State | Event | Next State | Actions on Transition |
|---|---|---|---|
| — (power on) | Board powered | BOOTING | Init peripherals, connect WiFi/MQTT |
| BOOTING | WiFi + MQTT connected | IDLE | Publish discovery, subscribe topics |
| IDLE | Button 1 pressed | MEASURING | Enumerate sensors, publish `session/state = active` |
| MEASURING | Button 1 pressed | IDLE | Publish `session/state = idle`, stop polling |
| MEASURING | Sensor ≥ target | ALERT | Publish `target_reached`, start LED 2 flash |
| ALERT | Button 1 pressed | IDLE | Publish `session/state = idle`, stop polling, stop LED flash |
| Any (except BOOT) | WiFi/MQTT lost | *(same state)* | Buffer readings, slow-blink LED 1, start reconnect |
| Any (except BOOT) | WiFi/MQTT restored | *(same state)* | Flush buffer, LED 1 steady ON |

---

## 7. Alerting Logic

### 7.1 Per-Sensor Target Reached

- The device compares the latest reading of each sensor against its stored target after every polling cycle.
- When a sensor's temperature meets or exceeds its target, the device:
  1. Publishes `true` to `meatometer/sensor/<id>/target_reached`.
  2. Transitions to the **ALERT** sub-state (LED 2 flashes at 500 ms ON / 500 ms OFF).
- The flash continues for the remainder of the session, even if the temperature subsequently drops below the target.
- If multiple sensors reach their targets at different times, the flash pattern persists (it is not retriggered per sensor).

### 7.2 Alert Reset

The LED 2 flash alert is cleared only when Button 1 is pressed (returning to IDLE). It is not cleared by temperature changes or by all targets being reached.



## 8. Offline / Reconnection Behaviour

### 8.1 WiFi / MQTT Connection Loss During Session

- If the WiFi or MQTT connection is lost during an active session (MEASURING or ALERT state):
  - The session continues running locally.
  - Temperature readings are stored in a local circular buffer in RAM (capacity: TBD, minimum 10 minutes of data for 4 sensors at 5-second intervals = 480 readings).
  - LED 1 blinks slowly (e.g. 1 s ON / 1 s OFF) to indicate connectivity loss.
- When the connection is restored:
  - Buffered readings are published to MQTT in chronological order before resuming live publishing.
  - LED 1 returns to steady ON.

### 8.2 Reconnection Strategy

- The firmware shall attempt WiFi/MQTT reconnection automatically with exponential back-off (initial: 5 s, max: 60 s).

---

## 9. Persistence (Non-Volatile Storage)

The following data shall be stored in flash (NVS) and survive power cycles:

| Data | Notes |
|---|---|
| WiFi SSID & Password | Set via BLE provisioning |
| MQTT broker address, port, credentials | Set via BLE provisioning |
| Target temperature per sensor (1–4) | Updated whenever a new value is received via MQTT |

---

## 10. Boot Sequence

The firmware starts automatically when the board is powered on. There is no software-controlled power-on via button.

### 10.1 Boot Steps

1. Board receives power; firmware starts executing.
2. Peripherals are initialised (ADC, GPIO, BLE stack, WiFi driver).
3. LED 1 blinks rapidly to indicate boot in progress; LED 2 OFF.
4. NVS is read: stored WiFi credentials, MQTT settings, and target temperatures are loaded.
5. **If no WiFi credentials are stored:** BLE provisioning advertisement starts. The user connects via BLE and provides SSID, password, and MQTT broker details. Credentials are saved to NVS.
6. Device connects to WiFi. On failure after 3 retries: LED 1 blinks rapidly (fast blink pattern distinct from boot blink); retries with exponential back-off.
7. MQTT client connects to the broker and registers LWT (`device/status = offline`).
8. `device/status = online` is published.
9. Home Assistant MQTT auto-discovery messages are published (retained).
10. Device subscribes to all `sensor/<id>/target` topics.
11. Device transitions to **IDLE** state. LED 1 ON (steady); LED 2 OFF.

### 10.2 Shutdown

The device has no software shutdown — it powers off when the board loses power. Before any unclean power loss the LWT mechanism ensures Home Assistant receives `device/status = offline` via the broker's LWT delivery.

For a graceful shutdown (e.g. via a future OTA or service command), the firmware shall:
1. Stop any active session (publish `session/state = idle`).
2. Publish `device/status = offline`.
3. Close the MQTT connection cleanly.
4. Disconnect from WiFi.



## 11. Non-Functional Requirements

| Requirement | Target |
|---|---|
| Temperature publishing latency | ≤ 5 seconds from measurement to MQTT broker |
| WiFi provisioning time | ≤ 60 seconds via BLE |
| WiFi reconnection time | ≤ 60 seconds after signal restoration |
| Buffered data capacity | ≥ 10 minutes of readings for 4 sensors |
| Sensor reading accuracy | Dependent on sensor type chosen (see Section 3.2) |
| Firmware update | Via USB (DFU) — OTA out of scope for v1.0 |

---

## 12. Open Questions / Decisions Required

| # | Question | Owner | Status |
|---|---|---|---|
| 1 | Final sensor type selection (NTC vs thermocouple) | Hardware lead | Open |
| 2 | Physical probe/connector type (e.g. 3.5 mm audio jack standard for meat probes) | Hardware lead | Open |
| 3 | MQTT broker address configuration method (BLE provisioning or hardcoded) | Firmware lead | Open |
| 4 | Local buffer storage location (RAM vs flash) and exact capacity | Firmware lead | Open |
| 5 | BLE provisioning app — use nRF Connect or build custom? | Software lead | Open |

---

## 13. Revision History

| Version | Date | Author | Notes |
|---|---|---|---|
| 1.0 | 2026-04-11 | — | Initial draft |
| 1.1 | 2026-04-11 | — | Revised button assignments (Button 1 = session toggle; Button 2 unused); firmware boots on power-on; replaced session lifecycle with full FSM (Section 6) |
