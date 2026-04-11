#pragma once

#include "hal_iface.h"
#include <zephyr/kernel.h>

void measure_temp_init(const hal_iface_t *hal, struct k_msgq *queue);
void measure_temp_start(void);
void measure_temp_stop(void);
