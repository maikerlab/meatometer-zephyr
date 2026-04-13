#pragma once

#include "sensor_iface.h"

const sensor_iface_t *sensor_mock_get_iface(void);
void sensor_mock_reset(void);
void sensor_mock_set_temp(float celsius);
