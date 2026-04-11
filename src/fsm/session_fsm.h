#pragma once

#include "app_events.h"
#include "hal_iface.h"
#include "mqtt_iface.h"
#include <stdbool.h>
#include <zephyr/kernel.h>

/** Initialize the session state machine
 * @param hal Pointer to HAL interface
 * @param mqtt Pointer to MQTT interface
 * @param queue Pointer to message queue where the state machine listens for
 * events
 */
void sm_init(const hal_iface_t *hal, const mqtt_iface_t *mqtt,
             struct k_msgq *queue);

/** Start the session state machine by spawning a seperate thread which receives
 * events sent to the queue, which was passed to the sm_init function.
 */
void sm_run(void);
/** Set the target temperature where an alarm should be triggered.
 * @param celsius Target temperature in degrees Celsius
 */
void sm_set_target_temp(float celsius);
bool sm_is_measuring(void);
int sm_handle_event(const app_event_t *evt);
