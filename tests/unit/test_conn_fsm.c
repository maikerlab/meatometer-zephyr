/*
 * Copyright (c) 2026 Maik Lorenz <maik.lorenz@pm.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_events.h"
#include "fsm/conn_fsm.h"
#include "mocks/ble_prov_mock.h"
#include "mocks/hal_mock.h"
#include "mocks/mqtt_mock.h"
#include "mocks/network_mock.h"
#include <zephyr/ztest.h>

/* ── Helper macros ───────────────────────────────────────────────────── */

#define SEND(event_type)                                                       \
	conn_fsm_handle_event(&(app_event_t){.type = (event_type)})

/* ── Helpers to reach specific states ────────────────────────────────── */

static void init_with_creds(void)
{
	hal_mock_reset();
	mqtt_mock_reset();
	network_mock_reset();
	ble_prov_mock_reset();
	network_mock_set_has_credentials(true);
	conn_fsm_init(hal_mock_get_iface(), network_mock_get_iface(),
		      mqtt_mock_get_iface(), ble_prov_mock_get_iface());
}

static void init_without_creds(void)
{
	hal_mock_reset();
	mqtt_mock_reset();
	network_mock_reset();
	ble_prov_mock_reset();
	network_mock_set_has_credentials(false);
	conn_fsm_init(hal_mock_get_iface(), network_mock_get_iface(),
		      mqtt_mock_get_iface(), ble_prov_mock_get_iface());
}

static void reach_mqtt_connecting(void)
{
	init_with_creds();
	SEND(EVT_WIFI_CONNECTED);
}

static void reach_online(void)
{
	reach_mqtt_connecting();
	/* Reset mocks so we can check what ONLINE entry does */
	ble_prov_mock_reset();
	mqtt_mock_reset();
	network_mock_reset();
	SEND(EVT_MQTT_CONNECTED);
}

/* ── Test Suite Setup ────────────────────────────────────────────────── */

static void before_each(void *f)
{
	(void)(f);
	/* Default: init with creds */
	init_with_creds();
}

ZTEST_SUITE(conn_fsm, NULL, NULL, before_each, NULL, NULL);

/* ── Tests ───────────────────────────────────────────────────────────── */

ZTEST(conn_fsm, test_init_with_creds_not_online)
{
	/* before_each already called init_with_creds */
	zassert_false(conn_fsm_is_online());
	zassert_true(network_mock_connect_stored_called(),
		     "connect_stored must be called when creds exist");
	zassert_true(hal_mock_led_blink_get(LED_STATUS),
		     "LED1 must blink in WIFI_CONNECTING");
}

ZTEST(conn_fsm, test_init_without_creds_starts_provisioning)
{
	init_without_creds();
	zassert_false(conn_fsm_is_online());
	zassert_true(ble_prov_mock_start_called(),
		     "BLE provisioning must start when no creds");
	zassert_false(network_mock_connect_stored_called(),
		      "connect_stored must NOT be called without creds");
	zassert_true(hal_mock_led_blink_get(LED_STATUS),
		     "LED1 must blink in PROVISIONING");
}

ZTEST(conn_fsm, test_wifi_connected_transitions_to_mqtt_connecting)
{
	/* Start from WIFI_CONNECTING (before_each) */
	mqtt_mock_reset();
	SEND(EVT_WIFI_CONNECTED);
	zassert_true(mqtt_mock_is_connected(),
		     "MQTT connect must be called after WiFi connects");
}

ZTEST(conn_fsm, test_wifi_connect_failed_transitions_to_provisioning)
{
	/* Start from WIFI_CONNECTING (before_each) */
	ble_prov_mock_reset();
	SEND(EVT_WIFI_CONNECT_FAILED);
	zassert_true(ble_prov_mock_start_called(),
		     "BLE provisioning must start after WiFi connect fails");
	zassert_true(hal_mock_led_blink_get(LED_STATUS),
		     "LED1 must blink in PROVISIONING");
}

ZTEST(conn_fsm, test_mqtt_ready_transitions_to_online)
{
	reach_mqtt_connecting();
	SEND(EVT_MQTT_CONNECTED);
	zassert_true(conn_fsm_is_online(), "Must be online after MQTT ready");
	zassert_true(hal_mock_led_get(LED_STATUS),
		     "LED1 must be steady ON when online");
}

ZTEST(conn_fsm, test_online_ble_stopped)
{
	reach_online();
	zassert_true(ble_prov_mock_stop_called(),
		     "BLE advertising must stop when online");
}

ZTEST(conn_fsm, test_wifi_disconnect_from_online_reconnects)
{
	reach_online();
	network_mock_reset();
	network_mock_set_has_credentials(true);
	SEND(EVT_WIFI_DISCONNECTED);
	zassert_false(conn_fsm_is_online(), "Must not be online after WiFi disconnect");
	zassert_true(network_mock_connect_stored_called(),
		     "connect_stored must be called to reconnect");
	zassert_true(hal_mock_led_blink_get(LED_STATUS),
		     "LED1 must blink while reconnecting");
}

ZTEST(conn_fsm, test_mqtt_disconnect_from_online_reconnects_mqtt)
{
	reach_online();
	mqtt_mock_reset();
	SEND(EVT_MQTT_DISCONNECTED);
	zassert_false(conn_fsm_is_online(),
		      "Must not be online after MQTT disconnect");
	zassert_true(mqtt_mock_is_connected(),
		     "MQTT connect must be called to reconnect");
}

ZTEST(conn_fsm, test_reconnect_button_from_online_starts_provisioning)
{
	reach_online();
	ble_prov_mock_reset();
	SEND(EVT_BTN_RECONNECT_WIFI);
	zassert_false(conn_fsm_is_online(),
		      "Must not be online after reconnect button");
	zassert_true(ble_prov_mock_start_called(),
		     "BLE provisioning must start on reconnect button");
}

ZTEST(conn_fsm, test_reconnect_button_from_wifi_connecting)
{
	/* Start from WIFI_CONNECTING (before_each) */
	ble_prov_mock_reset();
	SEND(EVT_BTN_RECONNECT_WIFI);
	zassert_true(ble_prov_mock_start_called(),
		     "BLE provisioning must start on reconnect button");
}

ZTEST(conn_fsm, test_reconnect_button_from_mqtt_connecting)
{
	reach_mqtt_connecting();
	ble_prov_mock_reset();
	SEND(EVT_BTN_RECONNECT_WIFI);
	zassert_true(ble_prov_mock_start_called(),
		     "BLE provisioning must start on reconnect button");
}

ZTEST(conn_fsm, test_provisioning_wifi_connected_continues_to_mqtt)
{
	init_without_creds();
	mqtt_mock_reset();
	SEND(EVT_WIFI_CONNECTED);
	zassert_true(mqtt_mock_is_connected(),
		     "MQTT connect must be called after provisioned WiFi connects");
}

ZTEST(conn_fsm, test_session_events_ignored)
{
	/* These events belong to session_fsm, conn_fsm should ignore them */
	SEND(EVT_BTN_MEASURE);
	SEND(EVT_TEMP_UPDATE);
	zassert_false(conn_fsm_is_online(),
		      "Session events must not change connectivity state");
}
