#pragma once
#include <stdbool.h>
#include "hal_iface.h"
#include "app_events.h"

void sm_init(const hal_iface_t *hal);
void sm_set_target_temp(float celsius);
bool sm_is_measuring(void);
int sm_handle_event(const app_event_t *evt);