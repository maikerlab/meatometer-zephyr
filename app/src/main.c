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
#include "comms/net_init.h"
#include "app/state_machine.h"
#include "app/event_handler.h"
#include "app/measure_temp.h"
#include "comms/wifi_mgr.h"
#include "comms/mqtt_mgr.h"

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

	// Initialize hardware and network interface
	const hal_iface_t *hal = hw_init();
	const network_iface_t *net = network_init();

	// Establish WiFi connection (blocks until connected or timeout)
	wifi_mgr_init(&app_event_queue);
	int retries = 5;
	while (retries--)
	{
		if (wifi_mgr_connect() != 0)
		{
			LOG_WRN("Failed to connect to WiFi, retrying in 1s... (%d retries left)", retries);
			k_sleep(K_SECONDS(1));
		}
	}

	k_sleep(K_SECONDS(1));
	mqtt_mgr_init(&app_event_queue);

	int ret;
	retries = 5;
	while (retries--)
	{
		ret = mqtt_mgr_connect();
		if (ret == 0)
		{
			break;
		}
		LOG_WRN("MQTT connect failed (%d), retrying in 1s... (%d left)", ret, retries);
		k_sleep(K_SECONDS(1));
	}

	if (ret != 0)
	{
		LOG_ERR("Failed to connect to MQTT broker after retries. Halting.");
		return -1;
	}

	// Initialize event handler
	event_handler_init(hal, &app_event_queue);
	// Initialize temperature measurement thread
	measure_temp_init(hal, &app_event_queue);

	// Initialize state machine with HAL and network interface
	sm_init(hal, net);

	return 0;
}
