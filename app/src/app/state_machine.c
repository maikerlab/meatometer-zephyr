// src/app/state_machine.c
#include "state_machine.h"
#include "app_events.h"
#include "app_config.h"
#include "hal_iface.h"
#include "measure_temp.h"
#include <zephyr/smf.h>
#include <zephyr/logging/log.h>
#include <stdatomic.h>

LOG_MODULE_REGISTER(state_machine, LOG_LEVEL_DBG);

/* ── Vorwärtsdeklarationen ────────────────────────────────────────────── */
static void state_off_entry(void *o);
static enum smf_state_result state_off_run(void *o);

static void state_idle_entry(void *o);
static enum smf_state_result state_idle_run(void *o);

static void state_measuring_entry(void *o);
static enum smf_state_result state_measuring_run(void *o);
static void state_measuring_exit(void *o);

static void state_done_entry(void *o);
static enum smf_state_result state_done_run(void *o);

/* ── State-Enum ──────────────────────────────────────────────────────── */
enum app_state
{
    ST_OFF,
    ST_IDLE,
    ST_MEASURING,
    ST_DONE
};

/* ── SMF State-Table ─────────────────────────────────────────────────── */
static const struct smf_state states[] = {
    [ST_OFF] = SMF_CREATE_STATE(state_off_entry, state_off_run, NULL, NULL, NULL),
    [ST_IDLE] = SMF_CREATE_STATE(state_idle_entry, state_idle_run, NULL, NULL, NULL),
    [ST_MEASURING] = SMF_CREATE_STATE(state_measuring_entry, state_measuring_run, state_measuring_exit, NULL, NULL),
    [ST_DONE] = SMF_CREATE_STATE(state_done_entry, state_done_run, NULL, NULL, NULL),
};

/* ── Kontext ─────────────────────────────────────────────────────────── */
typedef struct
{
    struct smf_ctx smf; /* Muss erstes Element sein */
    const hal_iface_t *hal;
    app_event_t current_event;
    float target_temp;
    float last_temp;
    bool led_power_on;
    bool led_measuring_on;
} sm_ctx_t;

static sm_ctx_t ctx;
static atomic_int measuring_active;

/* ── Öffentliche API ─────────────────────────────────────────────────── */

void sm_init(const hal_iface_t *hal)
{
    ctx.hal = hal;
    ctx.target_temp = APP_TARGET_TEMP_DEFAULT_C;
    ctx.last_temp = 0.0f;
    ctx.led_power_on = false;
    ctx.led_measuring_on = false;
    atomic_store(&measuring_active, 0);
    smf_set_initial(SMF_CTX(&ctx), &states[ST_OFF]);
}

void sm_set_target_temp(float celsius)
{
    ctx.target_temp = celsius;
}

bool sm_is_measuring(void)
{
    return atomic_load(&measuring_active) != 0;
}

int sm_handle_event(const app_event_t *evt)
{
    ctx.current_event = *evt;
    return smf_run_state(SMF_CTX(&ctx));
}

/* ── Helper function: Toggle LED ──────────────────────────────────────── */

static void toggle_led(sm_ctx_t *c, led_id_t led, bool *state)
{
    *state = !(*state);
    c->hal->led_set(led, *state);
    LOG_DBG("LED %d -> %s", led, *state ? "ON" : "OFF");
}

/* ── State: OFF ──────────────────────────────────────────────────────── */

static void state_off_entry(void *o)
{
    sm_ctx_t *c = (sm_ctx_t *)o;
    LOG_INF("→ OFF");
    c->hal->led_all_off();
    c->led_power_on = false;
    c->led_measuring_on = false;
    atomic_store(&measuring_active, 0);
}

static enum smf_state_result state_off_run(void *o)
{
    sm_ctx_t *c = (sm_ctx_t *)o;
    switch (c->current_event.type)
    {
    case EVT_BTN_POWER:
        /* Power Button pressed: go to IDLE, Power-LED on */
        smf_set_state(SMF_CTX(c), &states[ST_IDLE]);
        break;
    default:
        break;
    }
    return SMF_EVENT_HANDLED;
}

/* ── State: IDLE ─────────────────────────────────────────────────────── */

static void state_idle_entry(void *o)
{
    sm_ctx_t *c = (sm_ctx_t *)o;
    LOG_INF("→ IDLE");
    c->hal->led_all_off();
    c->led_power_on = true;
    c->led_measuring_on = false;
    c->hal->led_set(LED_POWER, true);
}

static enum smf_state_result state_idle_run(void *o)
{
    sm_ctx_t *c = (sm_ctx_t *)o;
    switch (c->current_event.type)
    {
    case EVT_BTN_POWER:
        toggle_led(c, LED_POWER, &c->led_power_on);
        if (!c->led_power_on)
        {
            smf_set_state(SMF_CTX(c), &states[ST_OFF]);
        }
        break;
    case EVT_BTN_MEASURE:
        smf_set_state(SMF_CTX(c), &states[ST_MEASURING]);
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
    c->led_measuring_on = true;
    c->hal->led_set(LED_POWER, true);
    c->hal->led_set(LED_MEASURING, true);
    measure_temp_start();
    atomic_store(&measuring_active, 1);
}

static enum smf_state_result state_measuring_run(void *o)
{
    sm_ctx_t *c = (sm_ctx_t *)o;
    switch (c->current_event.type)
    {
    case EVT_BTN_MEASURE:
        /* Measure-Toggle → Back to IDLE */
        smf_set_state(SMF_CTX(c), &states[ST_IDLE]);
        break;
    case EVT_BTN_POWER:
        /* Power-Toggle → All OFF */
        toggle_led(c, LED_POWER, &c->led_power_on);
        if (!c->led_power_on)
        {
            smf_set_state(SMF_CTX(c), &states[ST_OFF]);
        }
        break;
    case EVT_TEMP_UPDATE:
        /* Temperature update */
        c->last_temp = c->current_event.data.temperature;
        LOG_DBG("Temp: %.1f / %.1f °C", (double)c->last_temp, (double)c->target_temp);
        if (c->last_temp >= c->target_temp)
        {
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
    measure_temp_stop();
    atomic_store(&measuring_active, 0);
    c->led_measuring_on = false;
    c->hal->led_set(LED_MEASURING, false);
}

/* ── State: DONE ─────────────────────────────────────────────────────── */

static void state_done_entry(void *o)
{
    sm_ctx_t *c = (sm_ctx_t *)o;
    LOG_INF("→ DONE (%.1f °C reached)", (double)c->last_temp);
    c->hal->led_blink(LED_MEASURING, 500);
}

static enum smf_state_result state_done_run(void *o)
{
    sm_ctx_t *c = (sm_ctx_t *)o;
    switch (c->current_event.type)
    {
    case EVT_BTN_MEASURE:
        /* Measure-Toggle → Back to MEASURING (restart cycle) */
        smf_set_state(SMF_CTX(c), &states[ST_MEASURING]);
        break;
    case EVT_BTN_POWER:
        // Power-Toggle → All OFF
        smf_set_state(SMF_CTX(c), &states[ST_OFF]);
        break;
    default:
        break;
    }
    return SMF_EVENT_HANDLED;
}
