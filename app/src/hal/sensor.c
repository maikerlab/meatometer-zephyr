// tests/unit/mocks/hal_mock.c
#include "sensor.h"
#include "hal_iface.h"
#include <zephyr/drivers/spi.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>

LOG_MODULE_REGISTER(temperature, LOG_LEVEL_DBG);

static const struct device *const dev_temp0 = DEVICE_DT_GET(DT_ALIAS(temp0));

static float last_temp = 0.0f;

int sensor_init(void) {
    LOG_INF("Initializing sensors...");
    last_temp = 20.0f; // Simulated initial temperature
    return 0;
}

int sensor_temp_read(float *out)
{
    LOG_DBG("Reading temperature from sensor...");
    last_temp += 5.0f; // Simulate temperature change
    *out = last_temp;
    return 0;
}
