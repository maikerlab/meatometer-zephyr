#pragma once
#include "hal_iface.h"

int sensor_init(void);
int sensor_temp_read(float *out);
