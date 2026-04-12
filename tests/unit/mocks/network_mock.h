/*
 * Copyright (c) 2026 Maik Lorenz <maik.lorenz@pm.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "network_iface.h"
#include <stdbool.h>

const network_iface_t *network_mock_get_iface(void);
void network_mock_reset(void);
void network_mock_set_has_credentials(bool value);
bool network_mock_connect_stored_called(void);
bool network_mock_connect_called(void);
bool network_mock_disconnect_called(void);
