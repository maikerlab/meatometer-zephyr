#include "hw_init.h"
#include "sensor.h"
#include "led.h"
#include "button.h"

static const hal_iface_t iface = {
    .read_temp = sensor_temp_read,
    .led_set = led_set,
    .led_toggle = led_toggle,
    .led_blink = led_blink,
    .led_all_off = led_all_off,
    .btn_register_callback = button_register_callback,
};

const hal_iface_t *hw_init(void)
{
    sensor_init();
    led_init();
    button_init();
    return &iface;
}
