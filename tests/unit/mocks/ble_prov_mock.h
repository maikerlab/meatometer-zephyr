/*
 * Copyright (c) 2026 Maik Lorenz <maik.lorenz@pm.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "ble_prov_iface.h"
#include <stdbool.h>

const ble_prov_iface_t *ble_prov_mock_get_iface(void);
void ble_prov_mock_reset(void);
bool ble_prov_mock_start_called(void);
bool ble_prov_mock_stop_called(void);
