#pragma once
#include "sensor_iface.h"

/** Get the sensor interface for a dummy sensor which simulates temperature
 * readings.
 * @return Pointer to the sensor_iface_t.
 */
const sensor_iface_t *sensor_dummy_get_iface(void);
