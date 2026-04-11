#pragma once

#include "mqtt_iface.h"
#include <zephyr/kernel.h>

/**
 * @brief Gets the MQTT manager interface.
 * @param msgq Pointer to the message queue for posting MQTT events to
 * @return Pointer to the mqtt_iface_t struct
 */
const mqtt_iface_t *mqtt_get_iface(struct k_msgq *msgq);
