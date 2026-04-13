#pragma once

#include <stdbool.h>
#include <stdint.h>

void sensor_registry_mock_reset(void);
void sensor_registry_mock_set_connected_mask(uint8_t mask);
bool sensor_registry_mock_scan_called(void);
