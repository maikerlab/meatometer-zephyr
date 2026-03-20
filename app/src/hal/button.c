#include "button.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(button, LOG_LEVEL_DBG);

#define BTN_POWER_NODE    DT_ALIAS(btn_power)
#define BTN_MEASURE_NODE  DT_ALIAS(btn_measure)

static const struct gpio_dt_spec btns[BTN_COUNT] = {
    [BTN_POWER]   = GPIO_DT_SPEC_GET(BTN_POWER_NODE,   gpios),
    [BTN_MEASURE] = GPIO_DT_SPEC_GET(BTN_MEASURE_NODE, gpios),
};

static struct gpio_callback    gpio_cb_data[BTN_COUNT];
static btn_callback_t          user_callback;

static void gpio_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    /* Welchen Button hat der ISR ausgelöst? */
    for (btn_id_t i = 0; i < BTN_COUNT; i++) {
        if (cb == &gpio_cb_data[i] && user_callback) {
            user_callback(i);
            return;
        }
    }
}

int button_init(void)
{
    for (btn_id_t i = 0; i < BTN_COUNT; i++) {
        if (!gpio_is_ready_dt(&btns[i])) {
            return -ENODEV;
        }

        gpio_pin_configure_dt(&btns[i], GPIO_INPUT);
        gpio_pin_interrupt_configure_dt(&btns[i], GPIO_INT_EDGE_TO_ACTIVE);
        gpio_init_callback(&gpio_cb_data[i], gpio_isr, BIT(btns[i].pin));
        gpio_add_callback(btns[i].port, &gpio_cb_data[i]);
    }
    return 0;
}

void button_register_callback(btn_callback_t cb)
{
    user_callback = cb;
}