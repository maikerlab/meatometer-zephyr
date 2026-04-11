/**
 * @file ble_prov.h
 * @brief BLE provisioning — production implementation
 */

#pragma once

#include "ble_prov_iface.h"
#include <zephyr/kernel.h>

/**
 * @brief Get the production BLE provisioning interface.
 *
 * @param msgq Pointer to the FSM message queue for posting events.
 * @return Pointer to the ble_prov_iface_t.
 */
const ble_prov_iface_t *ble_prov_get_iface(struct k_msgq *msgq);
