#pragma once

#include "hal_iface.h"

const hal_iface_t *hal_mock_get_iface(void);

/* Test-Hilfsfunktionen */
void hal_mock_reset(void);
bool hal_mock_led_get(led_id_t id);
bool hal_mock_led_blink_get(led_id_t id);
float hal_mock_last_temp(void);
void hal_mock_set_temp(float celsius);
