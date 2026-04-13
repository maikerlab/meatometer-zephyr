#include "app_events.h"
#include "fsm/session_fsm.h"
#include "mocks/hal_mock.h"
#include "mocks/mqtt_mock.h"
#include "mocks/sensor_registry_mock.h"
#include "temperature.h"
#include <zephyr/ztest.h>

/* ── Hilfsmakros ─────────────────────────────────────────────────────── */

#define SEND(event_type) session_fsm_handle_event(&(app_event_t){.type = (event_type)})

#define SEND_TEMP(slot, celsius)                                                                   \
	session_fsm_handle_event(&(app_event_t){                                                   \
		.type = EVT_TEMP_UPDATE,                                                           \
		.data.temp.sensor_slot = (slot),                                                   \
		.data.temp.temperature = (celsius),                                                \
	})

/* ── Test Suite Setup ────────────────────────────────────────────────── */

static void before_each(void *f)
{
	(void)(f);
	hal_mock_reset();
	sensor_registry_mock_reset();
	sensor_registry_mock_set_connected_mask(0x01);
	session_fsm_init(hal_mock_get_iface(), mqtt_mock_get_iface());
}

ZTEST_SUITE(session_fsm, NULL, NULL, before_each, NULL, NULL);

/* ── Tests ───────────────────────────────────────────────────────────── */

ZTEST(session_fsm, test_initial_state_is_idle)
{
	/* Nach sm_init: kein Messen, alle LEDs aus */
	zassert_false(session_fsm_is_measuring());
	zassert_false(hal_mock_led_get(LED_MEASURING));
}

ZTEST(session_fsm, test_measure_starts_measuring)
{
	SEND(EVT_BTN_MEASURE); /* IDLE → MEASURING */

	zassert_true(session_fsm_is_measuring());
	zassert_true(hal_mock_led_get(LED_MEASURING), "Measuring LED must be on");
}

ZTEST(session_fsm, test_measure_toggle_stops_measuring)
{
	SEND(EVT_BTN_MEASURE); /* IDLE → MEASURING */
	SEND(EVT_BTN_MEASURE); /* MEASURING → IDLE */

	zassert_false(session_fsm_is_measuring());
	zassert_false(hal_mock_led_get(LED_MEASURING), "Measuring LED must be off");
}

ZTEST(session_fsm, test_temp_below_target_stays_measuring)
{
	session_fsm_set_target_temp(80.0f);
	SEND(EVT_BTN_MEASURE);

	SEND_TEMP(0, 79.9f);

	zassert_true(session_fsm_is_measuring(), "Must be measuring below target temperature");
}

ZTEST(session_fsm, test_temp_reached_goes_to_done)
{
	session_fsm_set_target_temp(80.0f);
	SEND(EVT_BTN_MEASURE);

	SEND_TEMP(0, 80.0f);

	zassert_false(session_fsm_is_measuring(), "Must stop measuring");
	zassert_true(hal_mock_led_blink_get(LED_MEASURING), "Done LED must be blinking");
}

ZTEST(session_fsm, test_measure_from_done_goes_to_idle)
{
	session_fsm_set_target_temp(80.0f);
	SEND(EVT_BTN_MEASURE);
	SEND_TEMP(0, 80.0f);   /* → DONE */
	SEND(EVT_BTN_MEASURE); /* DONE → IDLE */

	zassert_false(session_fsm_is_measuring());
	zassert_false(hal_mock_led_get(LED_MEASURING), "Measure LED must be off");
}

/* ── Sensor Detection Tests ──────────────────────────────────────────── */

ZTEST(session_fsm, test_btn_measure_with_sensors_starts_measuring)
{
	sensor_registry_mock_set_connected_mask(0x01);
	SEND(EVT_BTN_MEASURE);

	zassert_true(session_fsm_is_measuring());
	zassert_true(hal_mock_led_get(LED_MEASURING), "Measuring LED must be on");
	zassert_true(sensor_registry_mock_scan_called(), "scan must be called during detection");
}

ZTEST(session_fsm, test_btn_measure_no_sensors_stays_idle)
{
	sensor_registry_mock_set_connected_mask(0x00);
	SEND(EVT_BTN_MEASURE);

	zassert_false(session_fsm_is_measuring(), "Must stay idle with no sensors");
	zassert_false(hal_mock_led_get(LED_MEASURING), "Measuring LED must be off");
}

ZTEST(session_fsm, test_detecting_exposes_connected_mask)
{
	sensor_registry_mock_set_connected_mask(0x05);
	SEND(EVT_BTN_MEASURE);

	zassert_equal(session_fsm_get_connected_mask(), 0x05,
		      "Connected mask must match scanned sensors");
}

ZTEST(session_fsm, test_session_stop_clears_connected_mask)
{
	sensor_registry_mock_set_connected_mask(0x03);
	SEND(EVT_BTN_MEASURE); /* IDLE → DETECTING → MEASURING */
	SEND(EVT_BTN_MEASURE); /* MEASURING → IDLE */

	zassert_equal(session_fsm_get_connected_mask(), 0x00,
		      "Connected mask must be cleared after session stop");
}
