#pragma once

#include "sensor_iface.h"
#include <stdint.h>

/** Initialize the sensor registry. Must be called before any other registry
 * function. */
void sensor_registry_init(void);

/** Register a sensor in a specific slot.
 * @param slot Slot index (0 to SENSOR_MAX_COUNT-1)
 * @param iface Pointer to the sensor interface
 * @return 0 on success, -EINVAL if slot out of range
 */
int sensor_registry_register(uint8_t slot, const sensor_iface_t *iface);

/** Scan all registered slots. Calls init() on each and marks those
 * returning 0 as connected.
 * @return bitmask of connected sensors
 */
uint8_t sensor_registry_scan(void);

/** Get the sensor interface for a connected slot.
 * @param slot Slot index (0 to SENSOR_MAX_COUNT-1)
 * @return pointer to sensor_iface_t, or NULL if slot empty or not connected
 */
const sensor_iface_t *sensor_registry_get(uint8_t slot);

/** Get the connected mask from the last scan. */
uint8_t sensor_registry_get_connected_mask(void);
