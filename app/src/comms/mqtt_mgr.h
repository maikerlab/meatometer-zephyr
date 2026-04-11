#pragma once

#include <zephyr/kernel.h>
#include "mqtt_iface.h"

/**
 * Initializes the MQTT interface
 * @param msgq Pointer to the message queue for posting MQTT events
 * @return Pointer to the MQTT interface struct
 */
const mqtt_iface_t *mqtt_get_iface(struct k_msgq *msgq);
