#pragma once

#include "network_iface.h"

/**
 * Initializes the network interface and returns a pointer to a struct containing
 * a ready-to-use network_iface_t.
 */
network_iface_t *network_init(void);