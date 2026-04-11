#include "led.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led, LOG_LEVEL_DBG);

/* Devicetree-Aliases: led-power, led-measuring, led-done */
static const struct gpio_dt_spec leds[LED_COUNT] = {
    [LED_MEASURING] = GPIO_DT_SPEC_GET(DT_ALIAS(led_measuring), gpios),
    [LED_STATUS] = GPIO_DT_SPEC_GET(DT_ALIAS(led_status), gpios),
};

/* Blink-Work pro LED */
static struct k_work_delayable blink_work[LED_COUNT];
static bool blink_state[LED_COUNT];
static uint32_t blink_period_ms[LED_COUNT];

static void blink_handler(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    led_id_t id = (led_id_t)(dwork - blink_work); /* Pointer-Arithmetik */

    blink_state[id] = !blink_state[id];
    gpio_pin_set_dt(&leds[id], blink_state[id]);
    k_work_reschedule(dwork, K_MSEC(blink_period_ms[id] / 2));
}

int led_init(void)
{
    for (int i = 0; i < LED_COUNT; i++)
    {
        if (!gpio_is_ready_dt(&leds[i]))
            return -ENODEV;
        gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        k_work_init_delayable(&blink_work[i], blink_handler);
    }
    return 0;
}

void led_toggle(led_id_t id)
{
    k_work_cancel_delayable(&blink_work[id]);
    if (gpio_pin_toggle_dt(&leds[id]) < 0)
    {
        LOG_ERR("Err toggling LED");
    }
}

void led_set(led_id_t id, bool on)
{
    k_work_cancel_delayable(&blink_work[id]);
    gpio_pin_set_dt(&leds[id], on);
}

void led_blink(led_id_t id, uint32_t period_ms)
{
    blink_period_ms[id] = period_ms;
    blink_state[id] = false;
    k_work_reschedule(&blink_work[id], K_NO_WAIT);
}

void led_all_off(void)
{
    for (int i = 0; i < LED_COUNT; i++)
        led_set(i, false);
}