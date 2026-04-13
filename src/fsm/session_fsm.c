#include "session_fsm.h"
#include "app_config.h"
#include "app_events.h"
#include "hal_iface.h"
#include "mqtt_iface.h"
#include "network_iface.h"
#include "sensor/sensor_registry.h"
#include "temperature.h"
#include <stdatomic.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>

LOG_MODULE_REGISTER(session_fsm, LOG_LEVEL_DBG);

/* ── Forward declarations ────────────────────────────────────────────── */

static void state_idle_entry(void *o);
static enum smf_state_result state_idle_run(void *o);

static void state_detecting_entry(void *o);
static enum smf_state_result state_detecting_run(void *o);

static void state_measuring_entry(void *o);
static enum smf_state_result state_measuring_run(void *o);
static void state_measuring_exit(void *o);

static void state_done_entry(void *o);
static enum smf_state_result state_done_run(void *o);

/* ── State-Enum ──────────────────────────────────────────────────────── */

enum app_state {
	ST_IDLE,
	ST_DETECTING,
	ST_MEASURING,
	ST_DONE
};

/* ── SMF State-Table ─────────────────────────────────────────────────── */
static const struct smf_state states[] = {
	[ST_IDLE] = SMF_CREATE_STATE(state_idle_entry, state_idle_run, NULL, NULL, NULL),
	[ST_DETECTING] =
		SMF_CREATE_STATE(state_detecting_entry, state_detecting_run, NULL, NULL, NULL),
	[ST_MEASURING] = SMF_CREATE_STATE(state_measuring_entry, state_measuring_run,
					  state_measuring_exit, NULL, NULL),
	[ST_DONE] = SMF_CREATE_STATE(state_done_entry, state_done_run, NULL, NULL, NULL),
};

/* ── Context ─────────────────────────────────────────────────────────── */
typedef struct {
	struct smf_ctx smf; // Must be first element
	const hal_iface_t *hal;
	const mqtt_iface_t *mqtt;
	app_event_t current_event;
	float target_temp;
	bool led_measuring_on;
	uint8_t connected_mask;
} sm_ctx_t;

static sm_ctx_t ctx;
static atomic_int measuring_active;

/* ── Public API ─────────────────────────────────────────────────── */

void session_fsm_init(const hal_iface_t *hal, const mqtt_iface_t *mqtt)
{
	ctx.hal = hal;
	ctx.mqtt = mqtt;
	ctx.target_temp = APP_TARGET_TEMP_DEFAULT_C;
	ctx.led_measuring_on = false;
	ctx.connected_mask = 0;
	atomic_store(&measuring_active, 0);
	smf_set_initial(SMF_CTX(&ctx), &states[ST_IDLE]);
}

void session_fsm_set_target_temp(float celsius)
{
	ctx.target_temp = celsius;
}

bool session_fsm_is_measuring(void)
{
	return atomic_load(&measuring_active) != 0;
}

uint8_t session_fsm_get_connected_mask(void)
{
	return ctx.connected_mask;
}

int session_fsm_handle_event(const app_event_t *evt)
{
	ctx.current_event = *evt;
	return smf_run_state(SMF_CTX(&ctx));
}

/* ── State: IDLE ─────────────────────────────────────────────────────── */

static void state_idle_entry(void *o)
{
	sm_ctx_t *c = (sm_ctx_t *)o;
	LOG_INF("→ IDLE");
	c->hal->led_set(LED_MEASURING, false);
	c->led_measuring_on = false;
}

static enum smf_state_result state_idle_run(void *o)
{
	sm_ctx_t *c = (sm_ctx_t *)o;
	switch (c->current_event.type) {
	case EVT_BTN_MEASURE:
		smf_set_state(SMF_CTX(c), &states[ST_DETECTING]);
		break;
	default:
		break;
	}
	return SMF_EVENT_HANDLED;
}

/* ── State: DETECTING ─────────────────────────────────────────────────── */

static void state_detecting_entry(void *o)
{
	sm_ctx_t *c = (sm_ctx_t *)o;
	LOG_INF("→ DETECTING");

	uint8_t mask = sensor_registry_scan();
	c->connected_mask = mask;

	if (mask != 0) {
		LOG_INF("Detected %u sensor(s) (mask=0x%02x)", __builtin_popcount(mask), mask);
		smf_set_state(SMF_CTX(c), &states[ST_MEASURING]);
	} else {
		LOG_WRN("No sensors detected, returning to IDLE");
		smf_set_state(SMF_CTX(c), &states[ST_IDLE]);
	}
}

static enum smf_state_result state_detecting_run(void *o)
{
	sm_ctx_t *c = (sm_ctx_t *)o;
	switch (c->current_event.type) {
	case EVT_BTN_MEASURE:
		smf_set_state(SMF_CTX(c), &states[ST_IDLE]);
		break;
	default:
		break;
	}
	return SMF_EVENT_HANDLED;
}

/* ── State: MEASURING ────────────────────────────────────────────────── */

static void state_measuring_entry(void *o)
{
	sm_ctx_t *c = (sm_ctx_t *)o;
	LOG_INF("→ MEASURING (target: %.1f °C)", (double)c->target_temp);
	c->hal->led_set(LED_MEASURING, true);
	c->led_measuring_on = true;
	temperature_start();
	atomic_store(&measuring_active, 1);
}

static enum smf_state_result state_measuring_run(void *o)
{
	sm_ctx_t *c = (sm_ctx_t *)o;
	switch (c->current_event.type) {
	case EVT_BTN_MEASURE:
		/* Measure-Toggle → Back to IDLE */
		smf_set_state(SMF_CTX(c), &states[ST_IDLE]);
		break;
	case EVT_TEMP_UPDATE:
		/* Temperature update */
		uint8_t slot = c->current_event.data.temp.sensor_slot;
		float temp = c->current_event.data.temp.temperature;
		LOG_DBG("Received temperature update: slot %u = %.1f °C", slot, (double)temp);
		c->mqtt->publish_temperature(slot, temp);
		if (temp >= c->target_temp) {
			/* Target temperature reached → DONE */
			smf_set_state(SMF_CTX(c), &states[ST_DONE]);
		}
		break;
	default:
		break;
	}
	return SMF_EVENT_HANDLED;
}

static void state_measuring_exit(void *o)
{
	LOG_DBG("Exiting MEASURING state");
	sm_ctx_t *c = (sm_ctx_t *)o;
	c->connected_mask = 0;
	temperature_stop();
	atomic_store(&measuring_active, 0);
}

/* ── State: DONE ─────────────────────────────────────────────────────── */

static void state_done_entry(void *o)
{
	sm_ctx_t *c = (sm_ctx_t *)o;
	LOG_INF("→ DONE");
	c->hal->led_blink(LED_MEASURING, 500);
}

static enum smf_state_result state_done_run(void *o)
{
	sm_ctx_t *c = (sm_ctx_t *)o;
	switch (c->current_event.type) {
	case EVT_BTN_MEASURE:
		/* Measure-Toggle → Back to IDLE (must start a new cycle) */
		smf_set_state(SMF_CTX(c), &states[ST_IDLE]);
		break;
	default:
		break;
	}
	return SMF_EVENT_HANDLED;
}
