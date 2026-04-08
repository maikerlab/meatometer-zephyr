#pragma once

#include <stdbool.h>
#include <zephyr/kernel.h>

/**
 * Initializes the WiFi manager and registers net-management callbacks.
 * Must be called before wifi_connect().
 * @param event_queue Pointer to the app event message queue for posting WiFi events.
 */
void wifi_mgr_init(struct k_msgq *event_queue);

/**
 * Starts the Wi-Fi connection process with configured SSID and passphrase.
 * Result will be posted as EVT_WIFI_CONNECTED or EVT_WIFI_CONNECT_FAILED in the event queue.
 * @return 0 on success, 
 *   -ENOEXEC if connection initiation failed, 
 *   -ETIMEDOUT if connection timed out, 
 *   or -ECONNREFUSED if connection was refused.
 */
int wifi_mgr_connect(void);

/**
 * Disconnects the Wi-Fi connection.
 * @return 0 on success, 
 *   -ENOEXEC if disconnection failed,
 *   or -ENODEV if no Wi-Fi interface is available
 */
int wifi_mgr_disconnect(void);

/**
 * Returns true if currently connected and an IP address is available.
 */
bool wifi_mgr_is_connected(void);
