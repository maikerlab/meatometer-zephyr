// tests/unit/mocks/hal_mock.c
#include "sensor.h"
#include "hal_iface.h"
#include <zephyr/drivers/spi.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(sensor, LOG_LEVEL_DBG);

static const struct device *const dev_temp0 = DEVICE_DT_GET(DT_ALIAS(temp0));

static float last_temp = 0.0f;
static float min = 15.0f;
static float max = 40.0f;

static int sensor_temp_spi_read(const struct device *dev, double *val)
{
    int ret;

    ret = sensor_sample_fetch_chan(dev, SENSOR_CHAN_AMBIENT_TEMP);
    if (ret < 0)
    {
        LOG_ERR("Could not fetch temperature: %d\n", ret);
        return ret;
    }

    struct sensor_value value;
    ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &value);
    if (ret < 0)
    {
        LOG_ERR("Could not get temperature: %d\n", ret);
    }
    // TODO: comment in if sensor is available and ready to use via SPI
    //*val = sensor_value_to_double(&value);
    *val = 23.5;
    return ret;
}

int sensor_init(void)
{
    LOG_INF("Initializing sensors...");
    srand(k_uptime_get_32());
    last_temp = 20.0f; // Simulated initial temperature
    return 0;
}

int sensor_temp_read(float *out)
{
    LOG_DBG("Reading temperature from sensor...");

    // Publish random temperature values for testing
    float r = (float)rand() / (float)RAND_MAX;
    last_temp = min + r * (max - min);
    *out = last_temp;
    return 0;
}
