#pragma once

#include "hal_iface.h"
#include <zephyr/kernel.h>

/** Get the hardware abstraction layer interface.
 * @param evt_queue Pointer to message queue where button events should be
 * posted.
 * @return Pointer to the hal_iface_t.
 */
const hal_iface_t *hal_get_iface(struct k_msgq *msgq);
