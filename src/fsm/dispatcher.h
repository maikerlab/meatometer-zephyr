#pragma once
#include <zephyr/kernel.h>

void dispatcher_init(struct k_msgq *evt_queue);

/** Starts the dispatcher in a seperate thread which receives
 * events sent to the queue, which are dispatched to the state machines.
 */
void dispatcher_run(void);
