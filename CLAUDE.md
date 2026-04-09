# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# Project Overview

Meatometer is a Zephyr RTOS firmware project for a grill thermometer device built on the nRF7002 DK board. The device uses a dual-core nRF5340 MCU with Wi-Fi companion IC to measure meat core temperature via MAX31855 thermocouple sensors and report status via LED indicators.

# Commands

## Build

```bash
# Build project
west build -b nrf7002dk/nrf5340/cpuapp app -p auto

# Build with clean slate
west build -b nrf7002dk/nrf5340/cpuapp app -p always

# Build and flash
west build -b nrf7002dk/nrf5340/cpuapp app -p auto && west flash

# Menuconfig (interactive Kconfig)
west build -t menuconfig
```

## Flash

```bash
west flash
```

## Run Tests

```bash
# Run all tests
west test

# Run a specific test
west test -t app.state_machine

# Run with verbose output
west test -v

# Run with native_sim platform (no hardware)
west -b native_sim test -t app.state_machine
```

## Debug

```bash
# Start J-Link GDB server
JLinkGDBServer -device nRF5340_xxAA_APP -if SWD -speed 4000 -port 61234

# Connect to remote GDB server
target remote localhost:61234
```

## Serial Monitor

```bash
# Open serial monitor (default port/baud auto-detected)
west build -b nrf7002dk/nrf5340/cpuapp app -p auto
tio build/zephyr/subsys/bluetooth/host/prv_sw_ble_ctrl.log -b 115200
```

# Architecture

The application follows an event-driven architecture with the following components:

## Main Entry Point

- `app/src/main.c` — Initializes all subsystems:
  - Hardware initialization via `hal_iface_t` interface
  - State machine (`state_machine.c`)
  - Temperature measurement thread (`measure_temp.c`)
  - Event handler (`event_handler.c`)
  - Wi-Fi manager (`wifi_mgr.c`)

## Threads

Two application threads run alongside the main thread:

1. **AppState thread** (priority 1) — Runs the State Machine Framework (SMF) state machine
2. **MeasureTemp thread** (priority 7) — Polls temperature sensors via a binary semaphore

## State Machine

The state machine (`app/src/app/state_machine.c`) implements 4 states:

| State   | Description |
|---------|-------------|
| OFF     | Device powered off, all LEDs off, CPU sleeps |
| IDLE    | Powered on but not measuring, Power LED on |
| MEASURING | Actively measuring temperature, Measuring LED on |
| DONE    | Target temperature reached, Measuring LED blinks |

## Event Queue

All communication between threads uses a message queue (`K_MSGQ`) with these event types:

- `EVT_BTN_POWER` — Power button press
- `EVT_BTN_MEASURE` — Measure button press
- `EVT_TEMP_UPDATE` — New temperature reading
- `EVT_WIFI_CONNECTED` — Wi-Fi connected
- `EVT_WIFI_DISCONNECTED` — Wi-Fi disconnected
- `EVT_WIFI_CONNECT_FAILED` — Wi-Fi connection failed

## HAL Interface

Hardware abstraction layer defined in `app/include/hal_iface.h`:

- Sensor reading: `hal->read_temp(&temp)`
- LED control: `hal->led_set(id, on)`, `hal->led_toggle(id)`, `hal->led_blink(id, period)`
- Button callbacks: `hal->btn_register_callback(cb)`

This interface allows easy mocking for unit tests (see `app/tests/unit/mocks/hal_mock.c`).

## Wi-Fi Manager

Located in `app/src/comms/wifi_mgr.c`:

- Manages Wi-Fi connection lifecycle
- Uses Zephyr Network Management (NET_MGMT) APIs
- Registers callbacks for connection/disconnection events
- Configured via Kconfig values in `app/prj.conf`

# Code Style

Follows Zephyr RTOS coding guidelines:

- Indentation: tabs (not spaces)
- Brace style: K&R (opening brace on same line)
- Logging: Use `LOG_INF`, `LOG_ERR`, `LOG_DBG` from `<zephyr/logging/log.h>`
- License: SPDX Apache-2.0 header on all source files
- Guards: `#pragma once` or include guards
- Naming: snake_case for variables/functions/types, UPPER_CASE for macros
- Types: Typedef all structs with `_t` suffix

# Testing

Unit tests use Zephyr's `ztest` harness:

- Mocks: Hardware is mocked via `hal_mock.c` for unit testing
- Platform: `native_sim` for local testing, `nrf7002dk/nrf5340/cpuapp` for hardware
- Setup: Each test suite has `before_each` to reset mock state

To add a new test:

1. Add `ZTEST()` in `app/tests/unit/test_state_machine.c`
2. Add test case in `app/tests/unit/testcase.yaml`
3. Ensure mock or HAL implementation covers new functionality

# Configuration

## Kconfig

Main configuration in `app/prj.conf`:

- `CONFIG_SENSOR` — Enable sensor driver support
- `CONFIG_LOG` — Enable logging (sensor logs at debug level)
- `CONFIG_SMF` — Enable State Machine Framework
- `CONFIG_WIFI` — Enable Wi-Fi support
- `CONFIG_NET_MGMT` — Enable network management for Wi-Fi
- `CONFIG_NEWLIB_LIBC` — Enable newlib for printf support

## Devicetree

Board-specific overlays in `app/boards/`:

- `nrf7002dk_nrf5340_cpuapp.overlay` — Defines GPIOs for LEDs, buttons
- SPI configuration for MAX31855 thermocouple

# Environment

Required environment variable:

```bash
export ZEPHYR_BASE=/opt/nordic/ncs
```

# Notes

- The MAX31855 driver is not yet fully implemented; `hal/sensor.c` may need adaptation
- Wi-Fi credentials are set via Kconfig static credentials (see `prj.conf` comments)
- The device uses a single button for on/off (Power) and another for start/stop measurement
