# Product Requirements Document: Meatometer

**Version:** 1.3
**Date:** 2026-04-11
**Status:** Draft

---

## 1. Overview

### 1.1 Purpose

This document defines the requirements for the **Meatometer** — a smart multi-probe meat thermometer built on the Nordic Semiconductor nRF7002 DK. The device monitors up to four temperature sensors simultaneously during a grilling session, alerts the user when target temperatures are reached, and transmits readings to a Home Assistant instance via MQTT over WiFi.

### 1.2 Goals

- Provide real-time, multi-probe temperature monitoring during grilling sessions.
- Alert the user — via LED — when a probe reaches its target temperature.
- Integrate with Home Assistant for data visualisation and target temperature control.
- Operate fully offline; measurements are buffered locally and published when connectivity is available.
- Support WiFi provisioning via BLE, with manual re-provisioning on demand.

### 1.3 Out of Scope

- Mobile application development.
- Cloud connectivity (beyond a local MQTT broker).
- Battery management / low-power optimisation (v1.0 targets USB/mains power).
- Over-the-air firmware updates.

---

## 2. Prioritised Requirements Summary

Requirements are grouped by priority. Priority 1 covers the core device function and must be implemented first. Priority 2 adds target-temperature alerting. Priority 3 covers network connectivity and remote integration.

### Priority 1 — Session Management & Measuring

| ID | Requirement |
|---|---|
| P1-01 | The firmware shall start automatically when the board is powered on. No button press is required to boot. |
| P1-02 | After boot, the device shall be in IDLE mode. No sensor polling shall occur in IDLE. |
| P1-03 | Pressing Button 1 while in IDLE shall start a measurement session (transition to MEASURING mode). |
| P1-04 | Pressing Button 1 while in MEASURING mode shall stop the session and return to IDLE. |
| P1-05 | The device shall support between 1 and 4 temperature sensors connected simultaneously. |
| P1-06 | Connected sensors shall be detected automatically at the start of each session. Each sensor is identified by its physical port index (1–4). |
| P1-07 | During a session, all connected sensors shall be read every 5 seconds. |
| P1-08 | If a sensor disconnects mid-session, the device shall continue the session with the remaining sensors without faulting. |
| P1-09 | LED 2 shall be OFF in IDLE and ON (steady) during MEASURING. |
| P1-10 | Readings shall be stored in a local buffer throughout the session, regardless of network availability. The buffer shall hold at least 10 minutes of data for 4 sensors at 5-second intervals. When full, the oldest entries shall be overwritten. |

### Priority 2 — Target Temperature & Alerting

| ID | Requirement |
|---|---|
| P2-01 | A target temperature (°C) shall be configurable independently for each of the 4 sensor slots. |
| P2-02 | Target temperatures shall persist across power cycles. |
| P2-03 | If no target is configured for a sensor, no alert logic shall be active for that sensor. |
| P2-04 | After each reading cycle, the device shall compare each sensor's reading against its configured target. |
| P2-05 | When any sensor's temperature meets or exceeds its target, the device shall enter ALERT mode. |
| P2-06 | In ALERT mode, LED 2 shall flash. The session (polling and buffering) shall continue uninterrupted. |
| P2-07 | ALERT mode shall persist until the user stops the session (Button 1 to IDLE). It shall not be cleared automatically. |
| P2-08 | Multiple sensors reaching their targets at different times shall not re-trigger or reset the alert. |
| P2-09 | The device shall record per-sensor target-reached events for downstream publication (e.g. via MQTT when online). |

### Priority 3 — Connectivity, Provisioning & MQTT

| ID | Requirement |
|---|---|
| P3-01 | The device shall connect to a WiFi network and publish sensor data via MQTT to a user-configured local broker. |
| P3-02 | WiFi credentials and MQTT broker settings (host, port, username, password) shall be delivered to the device over BLE. Credentials shall be stored persistently. |
| P3-03 | On boot, if stored credentials exist, the device shall attempt WiFi connection before starting BLE advertising. |
| P3-04 | On boot, if no credentials are stored, or if the WiFi connection attempt fails, BLE advertising shall start. |
| P3-05 | Pressing Button 2 at any time shall trigger re-provisioning (e.g. after a network or credential change). |
| P3-06 | Re-provisioning shall not interrupt an active measurement session. Readings shall continue to be buffered during provisioning. |
| P3-07 | When connectivity is established or restored, buffered readings shall be published to MQTT in chronological order before live readings resume. |
| P3-08 | During a session, temperature readings shall be published to MQTT every 5 seconds per connected sensor. |
| P3-09 | Target setpoints received from the MQTT broker shall be stored persistently on the device. |
| P3-10 | Per-sensor target-reached events shall be published to MQTT. If offline at the time, the publish shall be queued and sent upon reconnection. |
| P3-11 | The device shall publish its session state (`active` / `idle`) to MQTT. |
| P3-12 | The device shall publish its connectivity status to MQTT and register an MQTT Last Will and Testament so ungraceful disconnects are reflected in the broker. |
| P3-13 | The device shall support Home Assistant MQTT auto-discovery so that sensor and setpoint entities appear in Home Assistant without manual configuration. |
| P3-14 | LED 1 shall reflect connectivity state: steady ON when online, slow blink when connecting or offline, double-blink during BLE provisioning. |

---

## 3. Hardware Platform

| Component | Details |
|---|---|
| Development Board | Nordic Semiconductor nRF7002 DK |
| Wireless | WiFi 6 (802.11ax) + BLE 5.x |
| Buttons | 2x onboard push buttons |
| LEDs | 2x onboard LEDs |
| Sensor Interface | Up to 4 temperature sensors |

### 3.1 Button Assignments

| Button | Function |
|---|---|
| Button 1 | **Session control** — toggles measurement session on/off |
| Button 2 | **Connectivity control** — triggers re-provisioning on demand |

### 3.2 LED Assignments

| LED | Indicates |
|---|---|
| LED 1 | Connectivity state |
| LED 2 | Session / alert state |

#### LED 1 — Connectivity States

| Pattern | Meaning |
|---|---|
| ON steady | Online (WiFi + MQTT connected) |
| Slow blink (1 s ON / 1 s OFF) | Connecting or offline (auto-reconnecting) |
| Double-blink | Provisioning (BLE advertising active) |
| Fast blink | Boot / initialisation in progress |

#### LED 2 — Session States

| Pattern | Meaning |
|---|---|
| OFF | Idle |
| ON steady | Measuring |
| Flashing (500 ms ON / 500 ms OFF) | Alert — one or more sensors have reached their target |

---

## 4. Temperature Sensors

### 4.1 Sensor Requirements

- The device shall support between 1 and 4 temperature sensors connected simultaneously.
- Sensors shall be identified by physical port index (1–4).
- Sensor detection shall occur automatically at the start of each session.
- A sensor disconnecting mid-session shall not terminate the session.
- The measurable temperature range shall cover at least 0 °C to 150 °C.

### 4.2 Sensor Type Selection

The sensor type has not yet been decided. The following options are under consideration:

| Type | Temp Range | Accuracy | Relative Cost |
|---|---|---|---|
| NTC Thermistor | -55 °C to +150 °C | +/-0.5–1 °C | Very low |
| Thermocouple (K-type) | -200 °C to +1260 °C | +/-1–2 °C | Low–Medium |
| PT100 / PT1000 RTD | -200 °C to +850 °C | +/-0.1–0.3 °C | Medium–High |

Final selection is pending (see Section 10, Open Questions).

---

## 5. Session Behaviour

### 5.1 Device Modes

| Mode | Description |
|---|---|
| **IDLE** | Device is on, no session is running. No sensor polling. |
| **MEASURING** | Session active. Sensors polled every 5 s. Readings buffered and published when online. |
| **ALERT** | Sub-mode of MEASURING. A sensor has reached its target. Session continues; LED 2 flashes. |

### 5.2 Mode Transitions

| From | Event | To |
|---|---|---|
| (power on) | Boot complete | IDLE |
| IDLE | Button 1 pressed | MEASURING |
| MEASURING | Button 1 pressed | IDLE |
| MEASURING | Any sensor >= target | ALERT |
| ALERT | Button 1 pressed | IDLE |

### 5.3 Session Rules

- Only sensors present at session start are included in the session.
- A sensor dropping out mid-session does not stop the session.
- Stopping the session clears any active alert.
- Session operation is independent of connectivity state.

---

## 6. Target Temperatures & Alerting

- Each sensor slot (1–4) has an independently configurable target temperature.
- Targets persist across power cycles and are retained between sessions.
- Targets can be updated via MQTT (from Home Assistant) at any time.
- If no target is set for a sensor, that sensor does not participate in alert evaluation.
- After each polling cycle, sensors with a configured target are evaluated.
- When a sensor reading meets or exceeds its target, ALERT mode is activated.
- ALERT mode persists until the session is stopped. Temperature changes do not clear it.
- Multiple sensors reaching their target does not re-trigger the alert.

---

## 7. Connectivity & MQTT

### 7.1 Offline-First Principle

The device does not require network connectivity to measure or alert. Sessions run fully offline. Readings are buffered locally and published when connectivity is established or restored.

### 7.2 WiFi Provisioning

- WiFi credentials and MQTT broker settings are provisioned over BLE.
- If credentials exist on boot, the device attempts WiFi connection before advertising via BLE.
- If the connection fails, or no credentials exist, BLE advertising starts.
- Button 2 triggers re-provisioning at any time without interrupting the session.
- Updated credentials received via BLE replace any previously stored credentials.

### 7.3 MQTT Topics

All topics are prefixed with `meatometer/`.

| Topic | Direction | Description |
|---|---|---|
| `meatometer/sensor/<id>/temperature` | Device to Broker | Current reading in °C |
| `meatometer/sensor/<id>/target` | Broker to Device | Target setpoint in °C |
| `meatometer/sensor/<id>/target_reached` | Device to Broker | `true` when sensor meets or exceeds its target |
| `meatometer/session/state` | Device to Broker | `active` or `idle` |
| `meatometer/device/status` | Device to Broker | `online` or `offline` (also used as LWT payload) |

`<id>` is the sensor's physical port index (1–4).

### 7.4 Publishing Rules

- Temperature readings are published every 5 seconds per connected sensor during an active session.
- On reconnection, buffered readings are published in chronological order before live readings resume.
- Target-reached events that occurred while offline are published upon reconnection.
- An MQTT Last Will and Testament is registered so the broker reflects `offline` on ungraceful disconnect.

### 7.5 Home Assistant Integration

- Sensor entities (one per connected sensor) shall appear automatically in Home Assistant, showing current temperature updated every 5 seconds.
- Setpoint entities (one per sensor slot) shall appear in Home Assistant as numeric inputs, allowing the user to configure target temperatures.
- All entities shall be created via MQTT auto-discovery without requiring manual YAML configuration.

---

## 8. Persistent Storage

The following shall be stored in non-volatile storage and survive power cycles:

| Data | Set By |
|---|---|
| WiFi SSID & password | BLE provisioning |
| MQTT broker host, port, username, password | BLE provisioning |
| Target temperature for each sensor slot (1–4) | MQTT (Home Assistant) |

---

## 9. Non-Functional Requirements

| ID | Requirement | Target |
|---|---|---|
| NFR-01 | Temperature reading interval | 5 seconds per sensor during a session |
| NFR-02 | Local reading buffer capacity | >= 10 minutes of data for 4 sensors at 5 s intervals |
| NFR-03 | Buffer overflow policy | Oldest entries overwritten; session is never interrupted |
| NFR-04 | WiFi reconnection | Automatic after connection loss; no user action required |
| NFR-05 | Future extensibility | A local display shall be connectable to show live readings without changes to session or alert behaviour |

---

## 10. Open Questions

| # | Question | Owner | Status |
|---|---|---|---|
| 1 | Final sensor type selection (NTC thermistor vs thermocouple vs RTD) | Hardware lead | Open |
| 2 | Physical probe connector type (e.g. 3.5 mm audio jack, common for food probes) | Hardware lead | Open |
| 3 | BLE provisioning client — use Nordic nRF Connect app or build a custom companion app? | Software lead | Open |
| 4 | Should buffered readings include an absolute timestamp, and if so, is NTP required? | Firmware lead | Open |
| 5 | Should the local reading buffer persist to flash for crash recovery, or RAM only? | Firmware lead | Open |

---

## 11. Revision History

| Version | Date | Author | Notes |
|---|---|---|---|
| 1.0 | 2026-04-11 | — | Initial draft |
| 1.1 | 2026-04-11 | — | Button 1 = session toggle; Button 2 unused; firmware boots on power-on; single FSM defined |
| 1.2 | 2026-04-11 | — | Dual-FSM architecture; offline-first design; Button 2 = re-provisioning; session buffered during provisioning |
| 1.3 | 2026-04-11 | — | Renamed to Meatometer; MQTT prefix updated to `meatometer/`; implementation details removed; prioritised requirements summary added (Section 2); document restructured as requirements-only |
