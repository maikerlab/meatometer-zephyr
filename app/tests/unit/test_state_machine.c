#include <zephyr/ztest.h>
#include "app/state_machine.h"
#include "app/measure_temp.h"
#include "app_events.h"
#include "mocks/hal_mock.h"

/* ── Hilfsmakros ─────────────────────────────────────────────────────── */

#define SEND(event_type) \
    sm_handle_event(&(app_event_t){.type = (event_type)})

#define SEND_TEMP(celsius)             \
    sm_handle_event(&(app_event_t){    \
        .type = EVT_TEMP_UPDATE,       \
        .data.temperature = (celsius), \
    })

/* ── Test Suite Setup ────────────────────────────────────────────────── */

static void before_each(void *f)
{
    (void)(f);
    hal_mock_reset();
    sm_init(hal_mock_get_iface());
}

ZTEST_SUITE(state_machine, NULL, NULL, before_each, NULL, NULL);

/* ── Tests ───────────────────────────────────────────────────────────── */

ZTEST(state_machine, test_initial_state_is_off)
{
    /* Nach sm_init: kein Messen, alle LEDs aus */
    zassert_false(sm_is_measuring());
    zassert_false(hal_mock_led_get(LED_POWER));
    zassert_false(hal_mock_led_get(LED_MEASURING));
}

ZTEST(state_machine, test_power_on_enables_power_led)
{
    SEND(EVT_BTN_POWER);

    zassert_false(sm_is_measuring());
    zassert_true(hal_mock_led_get(LED_POWER), "Power LED muss an sein");
    zassert_false(hal_mock_led_get(LED_MEASURING), "Measuring LED muss aus sein");
}

ZTEST(state_machine, test_power_toggle_goes_to_off)
{
    SEND(EVT_BTN_POWER); /* OFF → IDLE */
    SEND(EVT_BTN_POWER); /* IDLE → OFF */

    zassert_false(hal_mock_led_get(LED_POWER), "Power LED muss aus sein");
    zassert_false(sm_is_measuring());
}

ZTEST(state_machine, test_measure_starts_measuring)
{
    SEND(EVT_BTN_POWER);   /* OFF → IDLE */
    SEND(EVT_BTN_MEASURE); /* IDLE → MEASURING */

    zassert_true(sm_is_measuring());
    zassert_true(hal_mock_led_get(LED_MEASURING), "Measuring LED muss an sein");
}

ZTEST(state_machine, test_measure_toggle_stops_measuring)
{
    SEND(EVT_BTN_POWER);   /* OFF → IDLE     */
    SEND(EVT_BTN_MEASURE); /* IDLE → MEASURING */
    SEND(EVT_BTN_MEASURE); /* MEASURING → IDLE */

    zassert_false(sm_is_measuring());
    zassert_false(hal_mock_led_get(LED_MEASURING), "Measuring LED muss aus sein");
    zassert_true(hal_mock_led_get(LED_POWER), "Power LED muss noch an sein");
}

ZTEST(state_machine, test_temp_below_target_stays_measuring)
{
    sm_set_target_temp(80.0f);
    SEND(EVT_BTN_POWER);
    SEND(EVT_BTN_MEASURE);

    SEND_TEMP(79.9f);

    zassert_true(sm_is_measuring(), "Muss weiter messen unter Zieltemperatur");
}

ZTEST(state_machine, test_temp_reached_goes_to_done)
{
    sm_set_target_temp(80.0f);
    SEND(EVT_BTN_POWER);
    SEND(EVT_BTN_MEASURE);

    SEND_TEMP(80.0f);

    zassert_false(sm_is_measuring(), "Muss aufhören zu messen");
    zassert_true(hal_mock_led_blink_get(LED_MEASURING), "Done LED muss blinken");
}

ZTEST(state_machine, test_power_from_done_goes_to_measure)
{
    sm_set_target_temp(80.0f);
    SEND(EVT_BTN_POWER);
    SEND(EVT_BTN_MEASURE);
    SEND_TEMP(80.0f);      /* → DONE */
    SEND(EVT_BTN_MEASURE); /* DONE → IDLE */

    zassert_true(sm_is_measuring());
    zassert_true(hal_mock_led_get(LED_POWER), "Power LED muss an sein");
    zassert_true(hal_mock_led_get(LED_MEASURING), "Measure LED muss an sein");
}