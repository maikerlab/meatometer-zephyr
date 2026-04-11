/**
 * @file ble_prov_iface.h
 * @brief BLE provisioning interface
 *
 * Abstract interface for BLE-based WiFi provisioning. Production code uses
 * the NCS wifi_provisioning library; tests inject mocks.
 */

#pragma once

/** Callback invoked when BLE provisioning completes successfully. */
typedef void (*ble_prov_done_cb_t)(void);

/** Abstract BLE provisioning interface. */
typedef struct {
  int (*init)(void);
  int (*start)(void);
  int (*stop)(void);
} ble_prov_iface_t;
