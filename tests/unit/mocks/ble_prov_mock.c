/*
 * Copyright (c) 2026 Maik Lorenz <maik.lorenz@pm.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_prov_mock.h"

static bool start_called;
static bool stop_called;

static int mock_init(void) { return 0; }

static int mock_start(void) {
  start_called = true;
  return 0;
}

static int mock_stop(void) {
  stop_called = true;
  return 0;
}

static const ble_prov_iface_t mock_iface = {
    .init = mock_init,
    .start = mock_start,
    .stop = mock_stop,
};

const ble_prov_iface_t *ble_prov_mock_get_iface(void) { return &mock_iface; }

void ble_prov_mock_reset(void) {
  start_called = false;
  stop_called = false;
}

bool ble_prov_mock_start_called(void) { return start_called; }

bool ble_prov_mock_stop_called(void) { return stop_called; }
