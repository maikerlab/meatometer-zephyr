#pragma once

#include <stdbool.h>
#include <zephyr/kernel.h>

/**
 * Initializes the MQTT manager.
 * Must be called before mqtt_mgr_connect().
 * @param event_queue Pointer to the app event message queue.
 */
void mqtt_mgr_init(struct k_msgq *event_queue);

/**
 * Connects to the MQTT broker configured in app_config.h.
 * Blocks until CONNACK is received or timeout expires.
 * On success, starts a background polling thread for keep-alive.
 * @return 0 on success, negative errno on failure.
 */
int mqtt_mgr_connect(void);

/**
 * Disconnects from the MQTT broker and stops the polling thread.
 * @return 0 on success, negative errno on failure.
 */
int mqtt_mgr_disconnect(void);

/**
 * Publishes a temperature value to the configured MQTT topic.
 * @param temp_celsius Temperature in degrees Celsius.
 * @return 0 on success, negative errno on failure.
 */
int mqtt_mgr_publish_temperature(float temp_celsius);

/**
 * Returns true if currently connected to the MQTT broker.
 */
bool mqtt_mgr_is_connected(void);
