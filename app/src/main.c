/*
 * Copyright (c) 2026 Maik Lorenz <maik.lorenz@pm.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_version.h>
#include "app_events.h"
#include "app_config.h"
#include "hal/hw_init.h"
#include "fsm/session_fsm.h"
#include "app/event_handler.h"
#include "app/measure_temp.h"
#include "comms/wifi_mgr.h"
#include "comms/mqtt_mgr.h"
#include "comms/ble_prov.h"

LOG_MODULE_REGISTER(main);

/* size of stack area used by each thread */
#define TH_STACKSIZE 1024
/* scheduling priority used by AppState thread */
#define TH_APPSTATE_PRIORITY 1
/* scheduling priority used by MeasureTemp thread */
#define TH_TEMP_PRIORITY 7

/* Queue for app events */
K_MSGQ_DEFINE(app_event_queue, sizeof(app_event_t), APP_EVENT_QUEUE_DEPTH, 4);

int main(void)
{
	LOG_INF("Meatometer - v%s - arch: %s", APP_VERSION_STRING, CONFIG_ARCH);

	// Get all interfaces
	const hal_iface_t *hal = hal_get_iface();
	const network_iface_t *wifi = wifi_get_iface(&app_event_queue);
	const mqtt_iface_t *mqtt = mqtt_get_iface(&app_event_queue);
	const ble_prov_iface_t *ble_prov = ble_prov_get_iface(&app_event_queue);

	// Initialize subsystems
	hal->init();
	wifi->init();
	mqtt->init();
	ble_prov->init();

	// Initialize event handler
	event_handler_init(hal, &app_event_queue);
	// Initialize temperature measurement thread
	measure_temp_init(hal, &app_event_queue);

	// Initialize state machine with HAL and network interface
	sm_init(hal, mqtt);

	ble_prov->start();

	return 0;
}
