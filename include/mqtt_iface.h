#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Abstract MQTT interface.
 * In production code filled with real drivers, in tests with mock_mqtt.c
 */
typedef struct {
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
	int (*publish_temperature)(uint8_t sensor_slot, float temp_celsius);
	/** Publish HA MQTT discovery configs for sensors in the given bitmask.
	 *  Also publishes "online" to the availability topic.
	 * @param sensor_mask Bitmask of sensor slots to advertise (bit 0 = slot 0)
	 * @return 0 on success, negative error code on failure
	 */
	int (*publish_discovery)(uint8_t sensor_mask);
} mqtt_iface_t;
