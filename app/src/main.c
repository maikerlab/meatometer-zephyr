/*
 * Copyright (c) 2026 Maik Lorenz <maik.lorenz@pm.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_version.h>
#include "app_events.h"
#include "app_config.h"
#include "hal_iface.h"
#include "hal/led.h"
#include "hal/sensor.h"
#include "hal/hw_init.h"
#include "app/state_machine.h"
#include "app/event_handler.h"
#include "app/measure_temp.h"
#include "comms/wifi_mgr.h"

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

	const hal_iface_t *hal = hw_init();
	sm_init(hal);
	measure_temp_init(hal, &app_event_queue);
	event_handler_init(hal, &app_event_queue);

	// Establish WiFi connection (non-blocking)
	wifi_mgr_init(&app_event_queue);
	wifi_mgr_connect();

	// k_sleep(K_FOREVER);

	return 0;
}
