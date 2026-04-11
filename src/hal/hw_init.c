#include "hw_init.h"
#include "button.h"
#include "led.h"
#include "sensor.h"

static int hw_init(void) {
  sensor_init();
  led_init();
  button_init();
}

static const hal_iface_t iface = {
    .init = hw_init,
    .read_temp = sensor_temp_read,
    .led_set = led_set,
    .led_toggle = led_toggle,
    .led_blink = led_blink,
    .led_all_off = led_all_off,
    .btn_register_callback = button_register_callback,
};

const hal_iface_t *hal_get_iface(void) { return &iface; }
