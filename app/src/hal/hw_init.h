#pragma once

#include "hal_iface.h"

/**
 * Initialisiert alle Hardware-Komponenten und gibt
 * eine fertig verdrahtete hal_iface_t zurück.
 */
const hal_iface_t *hw_init(void);