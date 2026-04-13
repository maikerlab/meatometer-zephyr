#include "sensor_registry_mock.h"
#include "sensor_mock.h"
#include "sensor/sensor_registry.h"
#include "app_config.h"
#include <stddef.h>

static uint8_t mock_mask;
static bool mock_scan_called;

void sensor_registry_mock_reset(void)
{
	mock_mask = 0;
	mock_scan_called = false;
}

void sensor_registry_mock_set_connected_mask(uint8_t mask)
{
	mock_mask = mask;
}

bool sensor_registry_mock_scan_called(void)
{
	return mock_scan_called;
}

/* ── Real API implemented by mock ────────────────────────────────────── */

void sensor_registry_init(void)
{
}

int sensor_registry_register(uint8_t slot, const sensor_iface_t *iface)
{
	(void)slot;
	(void)iface;
	return 0;
}

uint8_t sensor_registry_scan(void)
{
	mock_scan_called = true;
	return mock_mask;
}

const sensor_iface_t *sensor_registry_get(uint8_t slot)
{
	if (slot >= SENSOR_MAX_COUNT) {
		return NULL;
	}
	if (mock_mask & (1U << slot)) {
		return sensor_mock_get_iface();
	}
	return NULL;
}

uint8_t sensor_registry_get_connected_mask(void)
{
	return mock_mask;
}
