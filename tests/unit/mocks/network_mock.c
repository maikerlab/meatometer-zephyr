/*
 * Copyright (c) 2026 Maik Lorenz <maik.lorenz@pm.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network_mock.h"
#include <string.h>

static bool mock_connected;
static bool mock_has_creds;
static bool connect_called;
static bool connect_stored_called;
static bool disconnect_called;

static int mock_init(void)
{
	return 0;
}

static int mock_disconnect(void)
{
	disconnect_called = true;
	mock_connected = false;
	return 0;
}

static bool mock_is_connected(void)
{
	return mock_connected;
}

static bool mock_has_credentials(void)
{
	return mock_has_creds;
}

static int mock_connect_stored(void)
{
	connect_stored_called = true;
	return 0;
}

static const network_iface_t mock_iface = {
	.init = mock_init,
	.connect = mock_connect_stored,
	.disconnect = mock_disconnect,
	.is_connected = mock_is_connected,
	.has_credentials = mock_has_credentials,
};

const network_iface_t *network_mock_get_iface(void)
{
	return &mock_iface;
}

void network_mock_reset(void)
{
	mock_connected = false;
	mock_has_creds = false;
	connect_called = false;
	connect_stored_called = false;
	disconnect_called = false;
}

void network_mock_set_has_credentials(bool value)
{
	mock_has_creds = value;
}

bool network_mock_connect_stored_called(void)
{
	return connect_stored_called;
}

bool network_mock_connect_called(void)
{
	return connect_called;
}

bool network_mock_disconnect_called(void)
{
	return disconnect_called;
}
