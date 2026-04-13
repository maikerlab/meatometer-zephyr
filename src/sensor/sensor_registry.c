#include "sensor_registry.h"
#include "app_config.h"
#include <errno.h>
#include <stddef.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_registry, LOG_LEVEL_DBG);

static const sensor_iface_t *slots[SENSOR_MAX_COUNT];
static uint8_t connected_mask;

void sensor_registry_init(void)
{
	for (int i = 0; i < SENSOR_MAX_COUNT; i++) {
		slots[i] = NULL;
	}
	connected_mask = 0;
}

int sensor_registry_register(uint8_t slot, const sensor_iface_t *iface)
{
	if (slot >= SENSOR_MAX_COUNT) {
		return -EINVAL;
	}
	slots[slot] = iface;
	LOG_DBG("Sensor registered in slot %u", slot);
	return 0;
}

uint8_t sensor_registry_scan(void)
{
	connected_mask = 0;

	for (uint8_t i = 0; i < SENSOR_MAX_COUNT; i++) {
		if (slots[i] == NULL) {
			continue;
		}
		int ret = slots[i]->init();
		if (ret == 0) {
			connected_mask |= (1U << i);
			LOG_INF("Sensor slot %u: connected", i);
		} else {
			LOG_WRN("Sensor slot %u: init failed (%d)", i, ret);
		}
	}

	LOG_INF("Scan complete: %u sensor(s) connected (mask=0x%02x)",
		__builtin_popcount(connected_mask), connected_mask);
	return connected_mask;
}

const sensor_iface_t *sensor_registry_get(uint8_t slot)
{
	if (slot >= SENSOR_MAX_COUNT) {
		return NULL;
	}
	if (!(connected_mask & (1U << slot))) {
		return NULL;
	}
	return slots[slot];
}

uint8_t sensor_registry_get_connected_mask(void)
{
	return connected_mask;
}
