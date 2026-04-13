/*
 * Copyright (c) 2026 Maik Lorenz <maik.lorenz@pm.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_config.h"
#include "app_events.h"
#include "comms/ble_prov.h"
#include "comms/mqtt_mgr.h"
#include "comms/wifi_mgr.h"
#include "fsm/conn_fsm.h"
#include "fsm/dispatcher.h"
#include "fsm/session_fsm.h"
#include "hal/hal.h"
#include "sensor/dummy.h"
#include "temperature.h"
#include <zephyr/app_version.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

/* size of stack area used by each thread */
#define TH_STACKSIZE 1024
/* scheduling priority used by AppState thread */
#define TH_APPSTATE_PRIORITY 1
/* scheduling priority used by MeasureTemp thread */
#define TH_TEMP_PRIORITY 7

/* Queue for app events */
K_MSGQ_DEFINE(app_event_queue, sizeof(app_event_t), APP_EVENT_QUEUE_DEPTH, 4);

int main(void) {
  LOG_INF("Meatometer - v%s - arch: %s", APP_VERSION_STRING, CONFIG_ARCH);

  // Get all interfaces
  const hal_iface_t *hal = hal_get_iface(&app_event_queue);
  const sensor_iface_t *sensor = sensor_dummy_get_iface();
  const network_iface_t *wifi = wifi_get_iface(&app_event_queue);
  const mqtt_iface_t *mqtt = mqtt_get_iface(&app_event_queue);
  const ble_prov_iface_t *ble_prov = ble_prov_get_iface(&app_event_queue);

  // Initialize subsystems
  hal->init();
  sensor->init();
  wifi->init();
  mqtt->init();
  ble_prov->init();

  // Initialize sensors
  temperature_init(sensor, &app_event_queue);

  // Initialize state machines
  session_fsm_init(hal, mqtt);
  conn_fsm_init(hal, wifi, mqtt, ble_prov);

  dispatcher_init(&app_event_queue);
  dispatcher_run();

  return 0;
}
