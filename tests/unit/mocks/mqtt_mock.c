#include "mqtt_mock.h"

static bool mock_initialized = false;
static bool mock_connected = false;
static float last_published_temp = 0.0f;
static uint8_t last_published_sensor_slot = 0;

static int mock_init(void)
{
	mock_initialized = true;
	return 0;
}

static int mock_connect(void)
{
	mock_connected = true;
	return 0;
}

static bool mock_is_connected(void)
{
	return mock_connected;
}

static int mock_disconnect(void)
{
	mock_connected = false;
	return 0;
}

static int mock_publish_temperature(uint8_t sensor_slot, float temp_celsius)
{
	last_published_sensor_slot = sensor_slot;
	last_published_temp = temp_celsius;
	return 0;
}

static uint8_t last_discovery_mask;

static int mock_publish_discovery(uint8_t sensor_mask)
{
	last_discovery_mask = sensor_mask;
	return 0;
}

static uint8_t last_subscribe_targets_mask;

static int mock_subscribe_targets(uint8_t sensor_mask)
{
	last_subscribe_targets_mask = sensor_mask;
	return 0;
}

static uint8_t last_target_state_slot;
static float last_target_state_value;

static int mock_publish_target_state(uint8_t sensor_slot, float target_celsius)
{
	last_target_state_slot = sensor_slot;
	last_target_state_value = target_celsius;
	return 0;
}

static const mqtt_iface_t mock_iface = {
	.init = mock_init,
	.connect = mock_connect,
	.is_connected = mock_is_connected,
	.disconnect = mock_disconnect,
	.publish_temperature = mock_publish_temperature,
	.publish_discovery = mock_publish_discovery,
	.subscribe_targets = mock_subscribe_targets,
	.publish_target_state = mock_publish_target_state,
};

/* ── Public API ─────────────────────────────────────────────────── */

const mqtt_iface_t *mqtt_mock_get_iface(void)
{
	return &mock_iface;
}

void mqtt_mock_reset(void)
{
	last_published_sensor_slot = 0;
	last_published_temp = 0.0f;
	last_discovery_mask = 0;
	last_subscribe_targets_mask = 0;
	last_target_state_slot = 0;
	last_target_state_value = 0.0f;
	mock_initialized = false;
	mock_connected = false;
}
bool mqtt_mock_is_initialized(void)
{
	return mock_initialized;
}
bool mqtt_mock_is_connected(void)
{
	return mock_connected;
}
float mqtt_mock_last_published_temp(void)
{
	return last_published_temp;
}
uint8_t mqtt_mock_last_published_sensor_slot(void)
{
	return last_published_sensor_slot;
}
uint8_t mqtt_mock_last_discovery_mask(void)
{
	return last_discovery_mask;
}
uint8_t mqtt_mock_last_target_state_slot(void)
{
	return last_target_state_slot;
}
float mqtt_mock_last_target_state_value(void)
{
	return last_target_state_value;
}
