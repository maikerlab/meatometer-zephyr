#include "session_fsm.h"
#include "app_config.h"
#include "app_events.h"
#include "hal_iface.h"
#include "mqtt_iface.h"
#include "network_iface.h"
#include "temperature.h"
#include <stdatomic.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>

LOG_MODULE_REGISTER(session_fsm, LOG_LEVEL_DBG);

#define EVENT_THREAD_STACK_SIZE 2048
#define EVENT_THREAD_PRIORITY 5

/* ── Forward declarations ────────────────────────────────────────────── */

static void state_idle_entry(void *o);
static enum smf_state_result state_idle_run(void *o);

static void state_measuring_entry(void *o);
static enum smf_state_result state_measuring_run(void *o);
static void state_measuring_exit(void *o);

static void state_done_entry(void *o);
static enum smf_state_result state_done_run(void *o);

/* ── State-Enum ──────────────────────────────────────────────────────── */

enum app_state { ST_IDLE, ST_MEASURING, ST_DONE };

/* ── SMF State-Table ─────────────────────────────────────────────────── */
static const struct smf_state states[] = {
    [ST_IDLE] =
        SMF_CREATE_STATE(state_idle_entry, state_idle_run, NULL, NULL, NULL),
    [ST_MEASURING] =
        SMF_CREATE_STATE(state_measuring_entry, state_measuring_run,
                         state_measuring_exit, NULL, NULL),
    [ST_DONE] =
        SMF_CREATE_STATE(state_done_entry, state_done_run, NULL, NULL, NULL),
};

/* ── Context ─────────────────────────────────────────────────────────── */
typedef struct {
  struct smf_ctx smf; // Must be first element
  const hal_iface_t *hal;
  const mqtt_iface_t *mqtt;
  app_event_t current_event;
  float target_temp;
  float last_temp;
  bool led_measuring_on;
} sm_ctx_t;

static sm_ctx_t ctx;
static atomic_int measuring_active;

K_THREAD_STACK_DEFINE(event_thread_stack, EVENT_THREAD_STACK_SIZE);
static struct k_thread event_thread_data;
static struct k_msgq *evt_queue;

/* ── Public API ─────────────────────────────────────────────────── */

void sm_init(const hal_iface_t *hal, const mqtt_iface_t *mqtt,
             struct k_msgq *queue) {
  ctx.hal = hal;
  ctx.mqtt = mqtt;
  evt_queue = queue;
  ctx.target_temp = APP_TARGET_TEMP_DEFAULT_C;
  ctx.last_temp = 0.0f;
  ctx.led_measuring_on = false;
  atomic_store(&measuring_active, 0);
  smf_set_initial(SMF_CTX(&ctx), &states[ST_IDLE]);
}

/** Event-Thread (receives events, calls state machine)
 * @param p1 Unused
 * @param p2 Unused
 * @param p3 Unused
 */
static void event_thread_fn(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  app_event_t evt;

  while (true) {
    k_msgq_get(evt_queue, &evt, K_FOREVER);
    LOG_DBG("Event received: %d", evt.type);
    sm_handle_event(&evt);
  }
}

void sm_run(void) {
  k_thread_create(&event_thread_data, event_thread_stack,
                  K_THREAD_STACK_SIZEOF(event_thread_stack), event_thread_fn,
                  NULL, NULL, NULL, EVENT_THREAD_PRIORITY, 0, K_NO_WAIT);
  k_thread_name_set(&event_thread_data, "event_thread");
}

void sm_set_target_temp(float celsius) { ctx.target_temp = celsius; }

bool sm_is_measuring(void) { return atomic_load(&measuring_active) != 0; }

int sm_handle_event(const app_event_t *evt) {
  ctx.current_event = *evt;
  return smf_run_state(SMF_CTX(&ctx));
}

/* ── State: IDLE ─────────────────────────────────────────────────────── */

static void state_idle_entry(void *o) {
  sm_ctx_t *c = (sm_ctx_t *)o;
  LOG_INF("→ IDLE");
  c->hal->led_set(LED_MEASURING, false);
  c->led_measuring_on = false;
}

static enum smf_state_result state_idle_run(void *o) {
  sm_ctx_t *c = (sm_ctx_t *)o;
  switch (c->current_event.type) {
  case EVT_BTN_MEASURE:
    smf_set_state(SMF_CTX(c), &states[ST_MEASURING]);
    break;
  default:
    break;
  }
  return SMF_EVENT_HANDLED;
}

/* ── State: MEASURING ────────────────────────────────────────────────── */

static void state_measuring_entry(void *o) {
  sm_ctx_t *c = (sm_ctx_t *)o;
  LOG_INF("→ MEASURING (target: %.1f °C)", (double)c->target_temp);
  c->hal->led_set(LED_MEASURING, true);
  c->led_measuring_on = true;
  temperature_start();
  atomic_store(&measuring_active, 1);
}

static enum smf_state_result state_measuring_run(void *o) {
  sm_ctx_t *c = (sm_ctx_t *)o;
  switch (c->current_event.type) {
  case EVT_BTN_MEASURE:
    /* Measure-Toggle → Back to IDLE */
    smf_set_state(SMF_CTX(c), &states[ST_IDLE]);
    break;
  case EVT_TEMP_UPDATE:
    /* Temperature update */
    c->last_temp = c->current_event.data.temperature;
    LOG_DBG("Temp: %.1f / %.1f °C", (double)c->last_temp,
            (double)c->target_temp);
    c->mqtt->publish_temperature(c->last_temp);
    if (c->last_temp >= c->target_temp) {
      /* Target temperature reached → DONE */
      smf_set_state(SMF_CTX(c), &states[ST_DONE]);
    }
    break;
  default:
    break;
  }
  return SMF_EVENT_HANDLED;
}

static void state_measuring_exit(void *o) {
  LOG_DBG("Exiting MEASURING state");
  sm_ctx_t *c = (sm_ctx_t *)o;
  (void)c; // Unused atm
  temperature_stop();
  atomic_store(&measuring_active, 0);
}

/* ── State: DONE ─────────────────────────────────────────────────────── */

static void state_done_entry(void *o) {
  sm_ctx_t *c = (sm_ctx_t *)o;
  LOG_INF("→ DONE (%.1f °C reached)", (double)c->last_temp);
  c->hal->led_blink(LED_MEASURING, 500);
}

static enum smf_state_result state_done_run(void *o) {
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
