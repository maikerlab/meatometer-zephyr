#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Abstract MQTT interface.
 * In production code filled with real drivers, in tests with mock_mqtt.c
 */
typedef struct
{
    /** Initializes the MQTT interface.
     * @return 0 on success, negative error code on failure
     */
    int (*init)(void);

    /** Connects to the MQTT broker.
     * @return 0 on success, negative error code on failure
     */
    int (*connect)(void);
    /** Checks if there is an active connection to the MQTT broker.
     * @return true if connected, false otherwise
     */
    bool (*is_connected)(void);
    /** Disconnects from the MQTT broker.
     * @return 0 on success, negative error code on failure
     */
    int (*disconnect)(void);
    /** Publishes a temperature value to the MQTT broker.
     * @param temp_celsius Temperature value in °C to publish
     * @return 0 on success, negative error code on failure
     */
    int (*publish_temperature)(float temp_celsius);
} mqtt_iface_t;
