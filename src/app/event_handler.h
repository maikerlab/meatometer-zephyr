#pragma once

#include "hal_iface.h"
#include <zephyr/kernel.h>

void event_handler_init(const hal_iface_t *hal, struct k_msgq *queue);