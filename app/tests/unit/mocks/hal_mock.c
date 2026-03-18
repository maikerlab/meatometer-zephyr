// tests/unit/mocks/hal_mock.c
#include "hal_mock.h"
#include "hal_iface.h"
#include <string.h>
#include <stdbool.h>

static float  mock_temp    = 20.0f;
static bool   mock_leds[LED_COUNT];
static bool   mock_blink[LED_COUNT];

void hal_mock_set_temp(float t) { mock_temp = t; }
bool hal_mock_led_get(led_id_t id) { return mock_leds[id]; }

static int mock_read_temp(float *out) { *out = mock_temp; return 0; }
static void mock_led_set(led_id_t id, bool on) { mock_leds[id] = on; }
static void mock_led_blink(led_id_t id, uint32_t ms) { mock_blink[id] = true; }
static void mock_led_all_off(void) { memset(mock_leds, 0, sizeof(mock_leds)); }

static const hal_iface_t mock_iface = {
    .read_temp   = mock_read_temp,
    .led_set     = mock_led_set,
    .led_blink   = mock_led_blink,
    .led_all_off = mock_led_all_off,
};

const hal_iface_t *hal_mock_get_iface(void) { return &mock_iface; }
