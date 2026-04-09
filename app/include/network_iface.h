#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Abstract network interface.
 * In production code filled with real drivers, in tests with hal_net.c
 */
typedef struct
{
    /** Connects to the configured WiFi network.
     * @return 0 on success, negative error code on failure
     */
    int (*wifi_connect)(void);
    /** Disconnects from the WiFi network.
     * @return 0 on success, negative error code on failure
     */
    int (*wifi_disconnect)(void);

    /** Connects to the MQTT broker.
     * @return 0 on success, negative error code on failure
     */
    int (*mqtt_connect)(void);
    /** Disconnects from the MQTT broker.
     * @return 0 on success, negative error code on failure
     */
    int (*mqtt_disconnect)(void);
    /** Publishes a temperature value to the MQTT broker.
     * @param temp_celsius Temperature value in °C to publish
     * @return 0 on success, negative error code on failure
     */
    int (*mqtt_publish_temperature)(float temp_celsius);
} network_iface_t;
