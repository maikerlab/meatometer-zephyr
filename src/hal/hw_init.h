#pragma once

#include "hal_iface.h"

/** Get the hardware abstraction layer interface.
 * @return Pointer to the hal_iface_t.
 */
const hal_iface_t *hal_get_iface(void);
