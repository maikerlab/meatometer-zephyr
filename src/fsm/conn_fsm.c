/*
 * Copyright (c) 2026 Maik Lorenz <maik.lorenz@pm.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "conn_fsm.h"
#include "app_events.h"
#include "ble_prov_iface.h"
#include "hal_iface.h"
#include "mqtt_iface.h"
#include "network_iface.h"
#include <stdatomic.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>

LOG_MODULE_REGISTER(conn_fsm, LOG_LEVEL_DBG);

#define LED_BLINK_SLOW_MS   1000
#define LED_BLINK_DOUBLE_MS 250

/* ── Forward declarations ────────────────────────────────────────────── */

static void state_provisioning_entry(void *o);
static enum smf_state_result state_provisioning_run(void *o);

static void state_wifi_connecting_entry(void *o);
static enum smf_state_result state_wifi_connecting_run(void *o);

static void state_mqtt_connecting_entry(void *o);
static enum smf_state_result state_mqtt_connecting_run(void *o);

static void state_online_entry(void *o);
static enum smf_state_result state_online_run(void *o);

/* ── State-Enum ──────────────────────────────────────────────────────── */

enum conn_state {
	ST_PROVISIONING,
	ST_WIFI_CONNECTING,
	ST_MQTT_CONNECTING,
	ST_ONLINE,
};

/* ── SMF State-Table ─────────────────────────────────────────────────── */

static const struct smf_state states[] = {
	[ST_PROVISIONING] = SMF_CREATE_STATE(state_provisioning_entry, state_provisioning_run, NULL,
					     NULL, NULL),
	[ST_WIFI_CONNECTING] = SMF_CREATE_STATE(state_wifi_connecting_entry,
						state_wifi_connecting_run, NULL, NULL, NULL),
	[ST_MQTT_CONNECTING] = SMF_CREATE_STATE(state_mqtt_connecting_entry,
						state_mqtt_connecting_run, NULL, NULL, NULL),
	[ST_ONLINE] = SMF_CREATE_STATE(state_online_entry, state_online_run, NULL, NULL, NULL),
};

/* ── Context ─────────────────────────────────────────────────────────── */

typedef struct {
	struct smf_ctx smf; /* must be first */
	const hal_iface_t *hal;
	const network_iface_t *wifi;
	const mqtt_iface_t *mqtt;
	const ble_prov_iface_t *ble_prov;
	app_event_t current_event;
} conn_fsm_ctx_t;

static conn_fsm_ctx_t ctx;
static atomic_int online_flag;

/* ── Public API ──────────────────────────────────────────────────────── */

void conn_fsm_init(const hal_iface_t *hal, const network_iface_t *wifi, const mqtt_iface_t *mqtt,
		   const ble_prov_iface_t *ble_prov)
{
	ctx.hal = hal;
	ctx.wifi = wifi;
	ctx.mqtt = mqtt;
	ctx.ble_prov = ble_prov;
	atomic_store(&online_flag, 0);

	if (wifi->has_credentials()) {
		LOG_INF("Stored credentials found, connecting to WiFi...");
		smf_set_initial(SMF_CTX(&ctx), &states[ST_WIFI_CONNECTING]);
		hal->led_blink(LED_STATUS, LED_BLINK_SLOW_MS);
		wifi->connect_stored();
	} else {
		LOG_INF("No stored credentials, starting BLE provisioning...");
		smf_set_initial(SMF_CTX(&ctx), &states[ST_PROVISIONING]);
		hal->led_blink(LED_STATUS, LED_BLINK_DOUBLE_MS);
		ble_prov->start();
	}
}

int conn_fsm_handle_event(const app_event_t *evt)
{
	ctx.current_event = *evt;
	return smf_run_state(SMF_CTX(&ctx));
}

bool conn_fsm_is_online(void)
{
	return atomic_load(&online_flag) != 0;
}

/* ── State: PROVISIONING ─────────────────────────────────────────────── */

static void state_provisioning_entry(void *o)
{
	conn_fsm_ctx_t *c = (conn_fsm_ctx_t *)o;
	LOG_INF("→ PROVISIONING");
	c->hal->led_blink(LED_STATUS, LED_BLINK_DOUBLE_MS);
	c->ble_prov->start();
	atomic_store(&online_flag, 0);
}

static enum smf_state_result state_provisioning_run(void *o)
{
	conn_fsm_ctx_t *c = (conn_fsm_ctx_t *)o;
	switch (c->current_event.type) {
	case EVT_WIFI_CONNECTED:
		smf_set_state(SMF_CTX(c), &states[ST_MQTT_CONNECTING]);
		break;
	case EVT_BTN_RECONNECT_WIFI:
		/* Already in provisioning, cancel BLE advertising */
		LOG_INF("Cancel BLE provisioning...");
		c->ble_prov->stop();
		smf_set_state(SMF_CTX(c), &states[ST_WIFI_CONNECTING]);
		break;
	default:
		break;
	}
	return SMF_EVENT_HANDLED;
}

/* ── State: WIFI_CONNECTING ──────────────────────────────────────────── */

static void state_wifi_connecting_entry(void *o)
{
	conn_fsm_ctx_t *c = (conn_fsm_ctx_t *)o;
	LOG_INF("→ WIFI_CONNECTING");
	c->hal->led_blink(LED_STATUS, LED_BLINK_SLOW_MS);
	c->wifi->connect_stored();
	atomic_store(&online_flag, 0);
}

static enum smf_state_result state_wifi_connecting_run(void *o)
{
	conn_fsm_ctx_t *c = (conn_fsm_ctx_t *)o;
	switch (c->current_event.type) {
	case EVT_WIFI_CONNECTED:
		smf_set_state(SMF_CTX(c), &states[ST_MQTT_CONNECTING]);
		break;
	case EVT_WIFI_CONNECT_FAILED:
		smf_set_state(SMF_CTX(c), &states[ST_PROVISIONING]);
		break;
	case EVT_BTN_RECONNECT_WIFI:
		smf_set_state(SMF_CTX(c), &states[ST_PROVISIONING]);
		break;
	default:
		break;
	}
	return SMF_EVENT_HANDLED;
}

/* ── State: MQTT_CONNECTING ──────────────────────────────────────────── */

static void state_mqtt_connecting_entry(void *o)
{
	conn_fsm_ctx_t *c = (conn_fsm_ctx_t *)o;
	LOG_INF("→ MQTT_CONNECTING");
	c->hal->led_blink(LED_STATUS, LED_BLINK_SLOW_MS);
	c->mqtt->connect();
	atomic_store(&online_flag, 0);
}

static enum smf_state_result state_mqtt_connecting_run(void *o)
{
	conn_fsm_ctx_t *c = (conn_fsm_ctx_t *)o;
	switch (c->current_event.type) {
	case EVT_MQTT_CONNECTED:
		smf_set_state(SMF_CTX(c), &states[ST_ONLINE]);
		break;
	case EVT_MQTT_DISCONNECTED:
		/* Retry MQTT connection */
		c->mqtt->connect();
		break;
	case EVT_WIFI_DISCONNECTED:
		smf_set_state(SMF_CTX(c), &states[ST_WIFI_CONNECTING]);
		break;
	case EVT_BTN_RECONNECT_WIFI:
		smf_set_state(SMF_CTX(c), &states[ST_PROVISIONING]);
		break;
	default:
		break;
	}
	return SMF_EVENT_HANDLED;
}

/* ── State: ONLINE ───────────────────────────────────────────────────── */

static void state_online_entry(void *o)
{
	conn_fsm_ctx_t *c = (conn_fsm_ctx_t *)o;
	LOG_INF("→ ONLINE");
	c->hal->led_set(LED_STATUS, true);
	c->ble_prov->stop();
	atomic_store(&online_flag, 1);
}

static enum smf_state_result state_online_run(void *o)
{
	conn_fsm_ctx_t *c = (conn_fsm_ctx_t *)o;
	switch (c->current_event.type) {
	case EVT_WIFI_DISCONNECTED:
		smf_set_state(SMF_CTX(c), &states[ST_WIFI_CONNECTING]);
		break;
	case EVT_MQTT_DISCONNECTED:
		smf_set_state(SMF_CTX(c), &states[ST_MQTT_CONNECTING]);
		break;
	case EVT_BTN_RECONNECT_WIFI:
		c->mqtt->disconnect();
		smf_set_state(SMF_CTX(c), &states[ST_PROVISIONING]);
		break;
	default:
		break;
	}
	return SMF_EVENT_HANDLED;
}
