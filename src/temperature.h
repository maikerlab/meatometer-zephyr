#pragma once
#include <zephyr/kernel.h>

/** Initialize the temperature measurement thread which posts measurements to
 * the queue. Sensors are read from the sensor registry.
 * @param queue Pointer to the app event message queue to post EVT_TEMP_UPDATE
 * events
 * @return 0 on success, negative error code on failure.
 */
int temperature_init(struct k_msgq *queue);
/** Starts temperature measurement */
void temperature_start(void);
/** Stops temperature measurement */
void temperature_stop(void);
