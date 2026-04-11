#include "hal.h"
#include <app_events.h>
#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hal, LOG_LEVEL_DBG);

#define BTN_MEASURE_NODE DT_ALIAS(btn_measure)
#define BTN_RECONNECT_WIFI_NODE DT_ALIAS(btn_reconnect_wifi)

/* ── Buttons ─────────────────────────────────────────────────── */

static const struct gpio_dt_spec btns[BTN_COUNT] = {
    [BTN_MEASURE] = GPIO_DT_SPEC_GET(BTN_MEASURE_NODE, gpios),
    [BTN_RECONNECT_WIFI] = GPIO_DT_SPEC_GET(BTN_RECONNECT_WIFI_NODE, gpios),
};

static struct gpio_callback gpio_cb_data[BTN_COUNT];
static btn_callback_t user_callback;
static struct k_msgq *evt_queue;

/** Button-Callback (runs in ISR context)
 * @param btn Pressed button ID
 */
static void on_button_pressed(btn_id_t btn) {
  LOG_DBG("Button pressed: %d", btn);
  const app_event_t evt_map[BTN_COUNT] = {
      [BTN_MEASURE] = {.type = EVT_BTN_MEASURE},
      [BTN_RECONNECT_WIFI] = {.type = EVT_BTN_RECONNECT_WIFI},
  };

  k_msgq_put(evt_queue, &evt_map[btn], K_NO_WAIT);
}

static void gpio_isr(const struct device *dev, struct gpio_callback *cb,
                     uint32_t pins) {
  /* Check which button triggered the interrupt */
  LOG_DBG("GPIO ISR triggered: pins=0x%08x", pins);
  for (btn_id_t i = 0; i < BTN_COUNT; i++) {
    if (cb == &gpio_cb_data[i]) {
      on_button_pressed(i);
      return;
    }
  }
}

static int button_init(void) {
  for (btn_id_t i = 0; i < BTN_COUNT; i++) {
    if (!gpio_is_ready_dt(&btns[i])) {
      return -ENODEV;
    }
    gpio_pin_configure_dt(&btns[i], GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&btns[i], GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&gpio_cb_data[i], gpio_isr, BIT(btns[i].pin));
    gpio_add_callback(btns[i].port, &gpio_cb_data[i]);
  }
  user_callback = on_button_pressed;
  return 0;
}

/* ── LEDs ───────────────────────────────────────────────────── */

/* Devicetree-Aliases: led-measuring, led-status */
static const struct gpio_dt_spec leds[LED_COUNT] = {
    [LED_MEASURING] = GPIO_DT_SPEC_GET(DT_ALIAS(led_measuring), gpios),
    [LED_STATUS] = GPIO_DT_SPEC_GET(DT_ALIAS(led_status), gpios),
};

/* Blink-Work pro LED */
static struct k_work_delayable blink_work[LED_COUNT];
static bool blink_state[LED_COUNT];
static uint32_t blink_period_ms[LED_COUNT];

static void blink_handler(struct k_work *work) {
  struct k_work_delayable *dwork = k_work_delayable_from_work(work);
  led_id_t id = (led_id_t)(dwork - blink_work); /* Pointer-Arithmetik */

  blink_state[id] = !blink_state[id];
  gpio_pin_set_dt(&leds[id], blink_state[id]);
  k_work_reschedule(dwork, K_MSEC(blink_period_ms[id] / 2));
}

void led_toggle(led_id_t id) {
  k_work_cancel_delayable(&blink_work[id]);
  if (gpio_pin_toggle_dt(&leds[id]) < 0) {
    LOG_ERR("Err toggling LED");
  }
}

void led_set(led_id_t id, bool on) {
  k_work_cancel_delayable(&blink_work[id]);
  gpio_pin_set_dt(&leds[id], on);
}

void led_blink(led_id_t id, uint32_t period_ms) {
  blink_period_ms[id] = period_ms;
  blink_state[id] = false;
  k_work_reschedule(&blink_work[id], K_NO_WAIT);
}

void led_all_off(void) {
  for (int i = 0; i < LED_COUNT; i++)
    led_set(i, false);
}

int led_init(void) {
  for (int i = 0; i < LED_COUNT; i++) {
    if (!gpio_is_ready_dt(&leds[i]))
      return -ENODEV;
    gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
    k_work_init_delayable(&blink_work[i], blink_handler);
  }
  return 0;
}

/* ── Internal ───────────────────────────────────────────────── */

static int hal_init(void) {
  LOG_DBG("Initializing HAL...");
  button_init();
  led_init();
  return 0;
}

static const hal_iface_t iface = {.init = hal_init,
                                  .led_set = led_set,
                                  .led_toggle = led_toggle,
                                  .led_blink = led_blink,
                                  .led_all_off = led_all_off};

/* ── Public API ─────────────────────────────────────────────── */

const hal_iface_t *hal_get_iface(struct k_msgq *msgq) {
  evt_queue = msgq;
  return &iface;
}
