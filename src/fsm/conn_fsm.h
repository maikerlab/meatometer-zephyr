/*
 * Copyright (c) 2026 Maik Lorenz <maik.lorenz@pm.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "app_events.h"
#include "ble_prov_iface.h"
#include "hal_iface.h"
#include "mqtt_iface.h"
#include "network_iface.h"

/** Initialize the connectivity state machine.
 * Checks for stored WiFi credentials and sets the initial state:
 * - Creds exist: starts WiFi auto-connect via connect_stored()
 * - No creds: starts BLE provisioning
 * @param hal Pointer to HAL interface (LED control)
 * @param wifi Pointer to network interface
 * @param mqtt Pointer to MQTT interface
 * @param ble_prov Pointer to BLE provisioning interface
 */
void conn_fsm_init(const hal_iface_t *hal, const network_iface_t *wifi, const mqtt_iface_t *mqtt,
		   const ble_prov_iface_t *ble_prov);

/** Handle an event synchronously.
 * @param evt Pointer to the event to handle
 * @return 0 on success, negative error code on failure
 */
int conn_fsm_handle_event(const app_event_t *evt);

/** Query if the device is fully online (WiFi + MQTT connected).
 * @return true if online, false otherwise
 */
bool conn_fsm_is_online(void);
