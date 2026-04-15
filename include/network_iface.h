#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Abstract network interface.
 * In production code filled with real drivers, in tests with mock_network.c
 */
typedef struct {
	/** Initializes the network interface.
	 * @return 0 on success, negative error code on failure
	 */
	int (*init)(void);
	/** Connects to WiFi using stored credentials.
	 * Non-blocking — result delivered via EVT_WIFI_CONNECTED /
	 * EVT_WIFI_CONNECT_FAILED.
	 * @return 0 on success (connection initiated), negative error code on failure
	 */
	int (*connect)(void);
	/** Disconnects from the network.
	 * @return 0 on success, negative error code on failure
	 */
	int (*disconnect)(void);
	/** Checks if the network is currently connected.
	 * @return true if connected, false otherwise
	 */
	bool (*is_connected)(void);
	/** Checks if stored WiFi credentials exist.
	 * @return true if credentials are stored, false otherwise
	 */
	bool (*has_credentials)(void);
} network_iface_t;
