#pragma once

#include "network_iface.h"
#include <stdbool.h>
#include <zephyr/kernel.h>

/**
 * @brief Gets the WiFi manager interface.
 * @param msgq Pointer to the message queue for posting network events to
 * @return Pointer to the network_iface_t struct
 */
const network_iface_t *wifi_get_iface(struct k_msgq *msgq);
