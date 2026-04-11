#pragma once
#include "hal_iface.h"
#include <stdbool.h>
#include <stdint.h>

int led_init(void);
void led_toggle(led_id_t id);
void led_set(led_id_t id, bool on);
void led_blink(led_id_t id, uint32_t period_ms);
void led_all_off(void);
