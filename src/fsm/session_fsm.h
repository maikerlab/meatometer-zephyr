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
/** Set the target temperature for a specific sensor slot.
 * @param sensor_slot Sensor slot index (0 to SENSOR_MAX_COUNT-1)
 * @param celsius Target temperature in degrees Celsius
 */
void session_fsm_set_target_temp(uint8_t sensor_slot, float celsius);
bool session_fsm_is_measuring(void);
/** Get the bitmask of sensors connected during the current session.
 * @return bitmask (bit N = slot N), 0 when not measuring.
 */
uint8_t session_fsm_get_connected_mask(void);
int session_fsm_handle_event(const app_event_t *evt);
