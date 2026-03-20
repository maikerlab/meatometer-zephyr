#include "hal_mock.h"
#include <string.h>

static float mock_temp = 20.0f;
static bool  mock_leds[LED_COUNT];
static bool  mock_blink[LED_COUNT];
static btn_callback_t mock_btn_cb;

/* ── Mock-Implementierungen ──────────────────────────────────────────── */

static int mock_read_temp(float *out)
{
    *out = mock_temp;
    return 0;
}

static void mock_led_set(led_id_t id, bool on)
{
    mock_leds[id]  = on;
    mock_blink[id] = false;   /* blink wird durch set überschrieben */
}

static void mock_led_blink(led_id_t id, uint32_t period_ms)
{
    (void)(period_ms);
    mock_blink[id] = true;
}

static void mock_led_all_off(void)
{
    memset(mock_leds,  0, sizeof(mock_leds));
    memset(mock_blink, 0, sizeof(mock_blink));
}

static void mock_btn_register_callback(btn_callback_t cb)
{
    mock_btn_cb = cb;
}

static const hal_iface_t mock_iface = {
    .read_temp             = mock_read_temp,
    .led_set               = mock_led_set,
    .led_blink             = mock_led_blink,
    .led_all_off           = mock_led_all_off,
    .btn_register_callback = mock_btn_register_callback,
};

/* ── Öffentliche API ─────────────────────────────────────────────────── */

const hal_iface_t *hal_mock_get_iface(void) { return &mock_iface; }

void hal_mock_reset(void)
{
    mock_temp   = 20.0f;
    mock_btn_cb = NULL;
    mock_led_all_off();
}

bool  hal_mock_led_get(led_id_t id)       { return mock_leds[id];  }
bool  hal_mock_led_blink_get(led_id_t id) { return mock_blink[id]; }
float hal_mock_last_temp(void)            { return mock_temp;       }
void  hal_mock_set_temp(float celsius)    { mock_temp = celsius;    }