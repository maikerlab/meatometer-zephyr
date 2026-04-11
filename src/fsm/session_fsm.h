#pragma once

#include "app_events.h"
#include "hal_iface.h"
#include "mqtt_iface.h"
#include <stdbool.h>

void sm_init(const hal_iface_t *hal, const mqtt_iface_t *mqtt);
void sm_set_target_temp(float celsius);
bool sm_is_measuring(void);
int sm_handle_event(const app_event_t *evt);