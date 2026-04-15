#include "app_events.h"
#include "fsm/session_fsm.h"
#include "mocks/hal_mock.h"
#include "mocks/mqtt_mock.h"
#include "mocks/sensor_registry_mock.h"
#include "mocks/temp_mock.h"
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

#define SEND_TARGET(slot, celsius)                                                                 \
	session_fsm_handle_event(&(app_event_t){                                                   \
		.type = EVT_TARGET_TEMP_SET,                                                       \
		.data.target.sensor_slot = (slot),                                                 \
		.data.target.temperature = (celsius),                                              \
	})

/* ── Test Suite Setup ────────────────────────────────────────────────── */

static void before_each(void *f)
{
	(void)(f);
	hal_mock_reset();
	sensor_registry_mock_reset();
	temp_mock_reset();
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
	session_fsm_set_target_temp(0, 80.0f);
	SEND(EVT_BTN_MEASURE);

	SEND_TEMP(0, 79.9f);

	zassert_true(session_fsm_is_measuring(), "Must be measuring below target temperature");
}

ZTEST(session_fsm, test_temp_reached_goes_to_done)
{
	session_fsm_set_target_temp(0, 80.0f);
	SEND(EVT_BTN_MEASURE);

	SEND_TEMP(0, 80.0f);

	zassert_false(session_fsm_is_measuring(), "Must stop measuring");
	zassert_true(hal_mock_led_blink_get(LED_MEASURING), "Done LED must be blinking");
}

ZTEST(session_fsm, test_measure_from_done_goes_to_idle)
{
	session_fsm_set_target_temp(0, 80.0f);
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

/* ── Per-sensor Target Temperature Tests ─────────────────────────────── */

ZTEST(session_fsm, test_reached_target_with_multiple_sensors)
{
	sensor_registry_mock_set_connected_mask(0x03); /* slots 0 and 1 */
	session_fsm_set_target_temp(0, 70.0f);
	session_fsm_set_target_temp(1, 90.0f);
	SEND(EVT_BTN_MEASURE); /* IDLE → DETECTING → MEASURING */

	/* Slot 0 below its target, slot 1 below its target */
	SEND_TEMP(0, 69.0f);
	SEND_TEMP(1, 89.0f);
	zassert_false(hal_mock_led_blink_get(LED_MEASURING), "Done LED must not be blinking");
	zassert_true(session_fsm_is_measuring(), "Must stay measuring when both below targets");

	/* Slot 1 reaches its own target → DONE */
	SEND_TEMP(1, 90.0f);
	zassert_true(hal_mock_led_blink_get(LED_MEASURING), "Done LED must be blinking");
	zassert_true(session_fsm_is_measuring(),
		     "Must stay measuring when one slot reaches target");

	/* Slot 2 reaches its own target → DONE */
	SEND_TEMP(0, 90.0f);
	zassert_true(hal_mock_led_blink_get(LED_MEASURING), "Done LED must be blinking");
	zassert_true(session_fsm_is_measuring(),
		     "Must stay measuring when both slots reach target");
}

ZTEST(session_fsm, test_target_change_during_measuring)
{
	session_fsm_set_target_temp(0, 80.0f);
	SEND(EVT_BTN_MEASURE); /* IDLE → DETECTING → MEASURING */

	SEND_TEMP(0, 79.0f);
	zassert_false(hal_mock_led_blink_get(LED_MEASURING), "Done LED must not be blinking");

	/* Raise target mid-session */
	session_fsm_set_target_temp(0, 100.0f);

	SEND_TEMP(0, 80.0f);
	zassert_false(hal_mock_led_blink_get(LED_MEASURING),
		      "Done LED must not be blinking below new target");

	SEND_TEMP(0, 100.0f);
	zassert_true(hal_mock_led_blink_get(LED_MEASURING),
		     "Done LED must be blinking if new target was reached");
}

ZTEST(session_fsm, test_target_set_event_updates_target)
{
	session_fsm_set_target_temp(0, 80.0f);
	SEND(EVT_BTN_MEASURE); /* IDLE → DETECTING → MEASURING */

	/* Receive target change via event (as from MQTT) */
	SEND_TARGET(0, 50.0f);

	/* Temperature at 50 should now trigger DONE */
	SEND_TEMP(0, 50.0f);
	zassert_true(hal_mock_led_blink_get(LED_MEASURING),
		     "Done LED must be blinking if new target was reached");

	/* Verify publish_target_state was called */
	zassert_equal(mqtt_mock_last_target_state_slot(), 0, "Target state slot must be 0");
	zassert_true(mqtt_mock_last_target_state_value() > 49.9f &&
			     mqtt_mock_last_target_state_value() < 50.1f,
		     "Target state value must be ~50.0");
}
