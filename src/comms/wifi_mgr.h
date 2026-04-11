#pragma once

#include "network_iface.h"
#include <stdbool.h>
#include <zephyr/kernel.h>

/**
 * Initializes the Wi-Fi interface
 * @param msgq Pointer to the message queue for posting network events
 * @return Pointer to the network interface struct
 */
const network_iface_t *wifi_get_iface(struct k_msgq *msgq);
