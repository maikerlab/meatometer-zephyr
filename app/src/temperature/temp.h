#pragma once
#include <zephyr/drivers/sensor.h>

/**
 * Reads a single measurement from the given temperature sensor device.
 *
 * @param dev the device to read from
 * @param val the value to store the read value to
 * @return 0 at success
 */
int temp_read(const struct device *dev, double *val);