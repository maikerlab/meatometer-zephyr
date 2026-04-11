#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Abstract network interface.
 * In production code filled with real drivers, in tests with mock_network.c
 */
typedef struct
{
    /** Initializes the network interface.
     * @return 0 on success, negative error code on failure
     */
    int (*init)(void);
    /** Connects to the configured network.
     * @return 0 on success, negative error code on failure
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
} network_iface_t;
