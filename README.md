# Meatometer

[![CI](https://github.com/maikerlab/meatometer-zephyr/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/maikerlab/meatometer-zephyr/actions/workflows/ci.yml)

A smart multi-probe meat thermometer built on the Nordic Semiconductor nRF7002 DK. Monitors up to four temperature sensors during a grilling session, alerts when target temperatures are reached, and publishes readings to Home Assistant via MQTT over WiFi. WiFi credentials and broker settings are provisioned over BLE.

## Features

### Priority 1 — Session Management & Measuring

- [x] Session FSM with IDLE / MEASURING / ALERT states
- [x] Button 1 toggles measurement session
- [ ] Up to 4 temperature sensors via MAX31855 SPI (currently using dummy sensor)
- [x] Automatic sensor detection at session start
- [x] 5-second polling interval during sessions
- [ ] Local ring buffer for offline reading storage (10 min capacity)

### Priority 2 — Target Temperature & Alerting

- [x] Target temperature comparison and ALERT mode
- [x] Per-sensor independent target temperatures
- [ ] Target temperature persistence across power cycles (NVS)

### Priority 3 — Connectivity, Provisioning & MQTT

- [x] WiFi connectivity manager with auto-reconnect
- [x] BLE provisioning for WiFi credentials
- [x] Connectivity FSM (provisioning / WiFi connecting / MQTT connecting / online)
- [x] MQTT broker connection and basic temperature publishing
- [x] Button 2 triggers re-provisioning
- [x] Event dispatcher connecting both FSMs
- [x] Per-sensor MQTT topic publishing
- [ ] Offline buffering with chronological replay on reconnect
- [x] MQTT subscription for target temperature control
- [ ] MQTT Last Will and Testament
- [x] Session state publishing via MQTT
- [x] Home Assistant MQTT auto-discovery

## Hardware

| Component | Details |
|-----------|---------|
| Board | [nRF7002 DK](https://www.nordicsemi.com/Products/Development-hardware/nRF7002-DK) (nRF5340 + nRF7002 WiFi 6) |
| Sensors | Up to 4 thermocouples via MAX31855 on SPI1 |
| Buttons | Button 1 — session control, Button 2 — re-provisioning |
| LEDs | LED 1 — connectivity state, LED 2 — session / alert state |

### LED Patterns

| LED | OFF | Steady ON | Slow blink | Double-blink | Fast blink (500 ms) |
|-----|-----|-----------|------------|--------------|---------------------|
| LED 1 (Connectivity) | — | Online | Connecting / offline | BLE provisioning | Boot |
| LED 2 (Session) | Idle | Measuring | — | — | Alert (target reached) |

## Architecture

The firmware is event-driven. Button presses, temperature readings, and connectivity changes are posted as events to a shared message queue (`k_msgq`). A dispatcher thread forwards events to two independent state machines:

- **Session FSM** (`src/fsm/session_fsm.c`) — manages IDLE / MEASURING / ALERT states and LED 2
- **Connectivity FSM** (`src/fsm/conn_fsm.c`) — manages BLE provisioning, WiFi, MQTT connection and LED 1

Hardware is abstracted behind interface structs (`hal_iface_t`, `sensor_iface_t`, `mqtt_iface_t`, `network_iface_t`, `ble_prov_iface_t`) for testability.

```
src/
  main.c              — entry point, initializes subsystems and starts dispatcher
  temperature.c       — temperature polling thread
  fsm/
    session_fsm.c     — session state machine (IDLE / MEASURING / ALERT)
    conn_fsm.c        — connectivity state machine (provisioning / WiFi / MQTT / online)
    dispatcher.c      — event dispatcher thread, forwards events to both FSMs
  comms/
    wifi_mgr.c        — WiFi connection manager
    mqtt_mgr.c        — MQTT client
    ble_prov.c        — BLE WiFi provisioning
  hal/
    hal.c             — LED and button abstraction (nRF DK library)
  sensor/
    dummy.c           — placeholder sensor (random values)
```

## Getting Started

### Prerequisites

- [nRF Connect SDK](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/installation.html) v3.2.3
- [West](https://docs.zephyrproject.org/latest/develop/west/index.html) build tool
- nRF7002 DK board (for flashing and hardware testing)
- `clang-format` (for code formatting)

### Clone and setup

```bash
git clone https://github.com/maikerlab/meatometer-zephyr.git
cd meatometer-zephyr
west init -l .
west update
git config core.hooksPath hooks
```

### Build

```bash
west build -b nrf7002dk/nrf5340/cpuapp -p auto
```

Use `-p always` for a pristine (clean) build.

### Flash

Connect the nRF7002 DK via USB and run:

```bash
west flash
```

### Run unit tests

Unit tests run on `native_sim` (no hardware required):

```bash
west twister -T tests/unit -p native_sim -v --inline-logs
```

## Developer Guidelines

- Follow [Zephyr coding guidelines](https://docs.zephyrproject.org/latest/contribute/coding_guidelines/index.html): tabs for indentation, K&R braces, snake_case, UPPER_CASE for macros
- Format all edited `.c` and `.h` files with `clang-format` before committing (config in `.clang-format`)
- A pre-commit hook enforces formatting — activate with `git config core.hooksPath hooks`
- Format all project files: `task format` (or `find src include tests -name '*.c' -o -name '*.h' | xargs clang-format -i`)
- Check formatting without modifying: `task lint`
- Use Zephyr logging (`LOG_INF`, `LOG_ERR`, `LOG_DBG`) instead of `printk`
- SPDX `Apache-2.0` license header on all source files
- `#pragma once` for header guards
- Typedef structs with `_t` suffix
- Devicetree overlays go in `boards/`, never edit SDK `.dts` files
- Kconfig in `prj.conf`, board-specific overrides in `boards/<board>.conf`

## Project Structure

```
CMakeLists.txt                          — build system
prj.conf                               — Kconfig
west.yml                               — west manifest (NCS v3.2.3)
.clang-format                           — code formatting config (Zephyr style)
Taskfile.yml                            — task runner (format, lint)
boards/
  nrf7002dk_nrf5340_cpuapp.overlay      — devicetree overlay (SPI, GPIOs)
hooks/
  pre-commit                            — clang-format pre-commit hook
include/                                — shared interface headers
src/                                    — application source (see Architecture)
tests/unit/                             — ztest unit tests and mocks
sysbuild/                               — sysbuild per-image configs
```

## License

Apache-2.0
