#include "mqtt_mock.h"

static bool mock_initialized = false;
static bool mock_connected = false;
static float last_published_temp = 0.0f;

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

static int mock_publish_temperature(float temp_celsius)
{
	last_published_temp = temp_celsius;
	return 0;
}

static const mqtt_iface_t mock_iface = {
	.init = mock_init,
	.connect = mock_connect,
	.is_connected = mock_is_connected,
	.disconnect = mock_disconnect,
	.publish_temperature = mock_publish_temperature,
};

/* ── Public API ─────────────────────────────────────────────────── */

const mqtt_iface_t *mqtt_mock_get_iface(void)
{
	return &mock_iface;
}

void mqtt_mock_reset(void)
{
	last_published_temp = 0.0f;
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
