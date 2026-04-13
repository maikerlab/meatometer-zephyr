#pragma once

#include "app_events.h"
#include "hal_iface.h"
#include "mqtt_iface.h"
#include <stdbool.h>
#include <zephyr/kernel.h>

/** Initialize the session state machine
 * @param hal Pointer to HAL interface
 * @param mqtt Pointer to MQTT interface
 */
void session_fsm_init(const hal_iface_t *hal, const mqtt_iface_t *mqtt);
/** Set the target temperature where an alarm should be triggered.
 * @param celsius Target temperature in degrees Celsius
 */
void session_fsm_set_target_temp(float celsius);
bool session_fsm_is_measuring(void);
/** Get the bitmask of sensors connected during the current session.
 * @return bitmask (bit N = slot N), 0 when not measuring.
 */
uint8_t session_fsm_get_connected_mask(void);
int session_fsm_handle_event(const app_event_t *evt);
