#pragma once
#include "hal_iface.h"
#include <sensor_iface.h>
#include <zephyr/kernel.h>

/** Initialize the sensor(s) and spawns a thread which posts measurements to the
 * queue.
 * @param sensor Pointer to the sensor interface to use for measurements
 * @param queue Pointer to the app event message queue to post EVT_TEMP_UPDATE
 * events
 * @return 0 on success, negative error code on failure.
 */
int temperature_init(const sensor_iface_t *sensor, struct k_msgq *queue);
/** Starts temperature measurement */
void temperature_start(void);
/** Stops temperature measurement */
void temperature_stop(void);
