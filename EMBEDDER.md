**EMBEDDER PROJECT CONTEXT**
<OVERVIEW>
Name = meatometer
Target MCU = nRF5340 (dual-core Arm Cortex-M33) + nRF7002 Wi-Fi companion IC
Board = nrf7002dk/nrf5340/cpuapp
Toolchain = nRF Connect SDK (NCS) v3.2.3 / West / Zephyr CMake
Toolchain Path = /opt/nordic/ncs
Debug Interface = jlink (onboard SEGGER J-Link on nRF7002 DK)
RTOS / SDK = Zephyr RTOS via nRF Connect SDK
Project Summary = Wi-Fi-connected grill thermometer using MAX31855 thermocouple sensor over SPI, with SMF state machine, BLE provisioning, MQTT telemetry, and button/LED UI.
</OVERVIEW>

<COMMANDS>
# --- Build / Compile --------------------------------------------------------
build_command = west build -b nrf7002dk/nrf5340/cpuapp -p auto

# --- Flash ------------------------------------------------------------------
flash_command = west flash

# --- Debug ------------------------------------------------------------------
gdb_server_command = JLinkGDBServer -device nRF5340_xxAA_APP -if SWD -speed 4000 -port 61234
gdb_server_host = localhost
gdb_server_port = 61234
# gdb_client_command =
target_connection = remote

# --- Serial Monitor ----------------------------------------------------------
serial_port = auto
serial_baudrate = 115200
serial_monitor_command = tio {port} -b {baud}
serial_monitor_interactive = true
serial_encoding = ascii
serial_startup_commands = []
</COMMANDS>

# Project Overview

Meatometer is a Wi-Fi-connected grill thermometer built on the nRF7002 DK. It reads thermocouple temperatures via a MAX31855 SPI ADC, runs a Zephyr SMF state machine for session control (IDLE / MEASURING / ALERT), and publishes readings over MQTT. Wi-Fi credentials and broker settings are provisioned via BLE. The firmware is event-driven, with a message queue connecting button ISRs, temperature polling, connectivity events, and the state machine.

Architecture:
- `src/main.c` — entry point, initializes all subsystem interfaces and starts FSM
- `src/fsm/` — session state machine (`session_fsm.c`) using Zephyr SMF
- `src/comms/` — Wi-Fi manager, MQTT manager, BLE provisioning
- `src/hal/` — hardware abstraction (LEDs, buttons) behind `hal_iface_t` interface
- `src/sensor/` — sensor abstraction behind `sensor_iface_t` (currently `dummy.c` placeholder)
- `src/temperature.c` — temperature processing and polling thread
- `include/` — shared interfaces (`hal_iface.h`, `sensor_iface.h`, `mqtt_iface.h`, `network_iface.h`, `ble_prov_iface.h`, `app_config.h`, `app_events.h`)
- `boards/` — board-specific devicetree overlays and Kconfig

Key peripherals: SPI1 (MAX31855 thermocouple ADC on P0.06/07/26, CS P0.25), GPIOs for 2 LEDs and 2 buttons, Wi-Fi via nRF7002, BLE for provisioning.

# Bash Commands

```bash
# Build
west build -b nrf7002dk/nrf5340/cpuapp -p auto

# Build (pristine)
west build -b nrf7002dk/nrf5340/cpuapp -p always

# Flash
west flash

# Build and flash
west build -b nrf7002dk/nrf5340/cpuapp -p auto && west flash

# Menuconfig (interactive Kconfig)
west build -t menuconfig

# Clean build directory
rm -rf build

# Run unit tests (native_sim)
west test
```

# Code Style

Follow [Zephyr coding guidelines](https://docs.zephyrproject.org/latest/contribute/coding_guidelines/index.html):
- Indentation: tabs (not spaces)
- Brace style: K&R (opening brace on same line for functions, structs, control flow)
- Use Zephyr logging (`LOG_INF`, `LOG_ERR`, `LOG_DBG`) instead of `printk`
- Use Zephyr kernel APIs (`k_msgq`, `k_thread`, `k_sleep`, etc.)
- SPDX license header (`Apache-2.0`) at top of every source file
- `#pragma once` for header guards
- Snake_case for variables, functions, types; UPPER_CASE for macros and constants
- Typedef structs with `_t` suffix (e.g. `app_event_t`, `hal_iface_t`)
- Keep HAL abstracted behind interface structs (`hal_iface_t`, `sensor_iface_t`, `mqtt_iface_t`, `network_iface_t`, `ble_prov_iface_t`) for testability
- Devicetree overlays in `boards/`, never edit SDK `.dts` files
- Kconfig in `prj.conf`, board-specific overrides in `boards/<board>.conf`
- Sysbuild config in `sysbuild.conf`, per-image overrides in `sysbuild/`
