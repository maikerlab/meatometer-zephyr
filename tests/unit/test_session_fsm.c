#include "app_events.h"
#include "fsm/session_fsm.h"
#include "mocks/hal_mock.h"
#include "mocks/mqtt_mock.h"
#include <zephyr/ztest.h>
#include "temperature.h"

/* ── Hilfsmakros ─────────────────────────────────────────────────────── */

#define SEND(event_type) sm_handle_event(&(app_event_t){.type = (event_type)})

#define SEND_TEMP(celsius)                                                     \
  sm_handle_event(&(app_event_t){                                              \
      .type = EVT_TEMP_UPDATE,                                                 \
      .data.temperature = (celsius),                                           \
  })

/* ── Test Suite Setup ────────────────────────────────────────────────── */

static void before_each(void *f) {
  (void)(f);
  hal_mock_reset();
  // TODO: create mock for mqtt iface and pass as 2nd argument
  sm_init(hal_mock_get_iface(), mqtt_mock_get_iface());
}

ZTEST_SUITE(session_fsm, NULL, NULL, before_each, NULL, NULL);

/* ── Tests ───────────────────────────────────────────────────────────── */

ZTEST(session_fsm, test_initial_state_is_idle) {
  /* Nach sm_init: kein Messen, alle LEDs aus */
  zassert_false(sm_is_measuring());
  zassert_false(hal_mock_led_get(LED_MEASURING));
}

ZTEST(session_fsm, test_measure_starts_measuring) {
  SEND(EVT_BTN_MEASURE); /* IDLE → MEASURING */

  zassert_true(sm_is_measuring());
  zassert_true(hal_mock_led_get(LED_MEASURING), "Measuring LED must be on");
}

ZTEST(session_fsm, test_measure_toggle_stops_measuring) {
  SEND(EVT_BTN_MEASURE); /* IDLE → MEASURING */
  SEND(EVT_BTN_MEASURE); /* MEASURING → IDLE */

  zassert_false(sm_is_measuring());
  zassert_false(hal_mock_led_get(LED_MEASURING), "Measuring LED must be off");
}

ZTEST(session_fsm, test_temp_below_target_stays_measuring) {
  sm_set_target_temp(80.0f);
  SEND(EVT_BTN_MEASURE);

  SEND_TEMP(79.9f);

  zassert_true(sm_is_measuring(), "Must be measuring below target temperature");
}

ZTEST(session_fsm, test_temp_reached_goes_to_done) {
  sm_set_target_temp(80.0f);
  SEND(EVT_BTN_MEASURE);

  SEND_TEMP(80.0f);

  zassert_false(sm_is_measuring(), "Must stop measuring");
  zassert_true(hal_mock_led_blink_get(LED_MEASURING),
               "Done LED must be blinking");
}

ZTEST(session_fsm, test_measure_from_done_goes_to_idle) {
  sm_set_target_temp(80.0f);
  SEND(EVT_BTN_MEASURE);
  SEND_TEMP(80.0f);      /* → DONE */
  SEND(EVT_BTN_MEASURE); /* DONE → IDLE */

  zassert_false(sm_is_measuring());
  zassert_false(hal_mock_led_get(LED_MEASURING), "Measure LED must be off");
}