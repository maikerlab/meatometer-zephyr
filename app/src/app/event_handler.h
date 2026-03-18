#pragma once

#include <zephyr/kernel.h>
#include "hal_iface.h"

void event_handler_init(const hal_iface_t *hal, struct k_msgq *queue);