#include "max6675.h"
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <sensor_iface.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(max6675_sensor, LOG_LEVEL_DBG);

const struct device *const dev_temp0 = DEVICE_DT_GET_ONE(maxim_max6675);
struct sensor_value val;

int max6675_init(void)
{
	LOG_INF("MAX6675: init");
	if (!device_is_ready(dev_temp0)) {
		LOG_ERR("sensor: device not ready.");
		return 1;
	}
	return 0;
}

static int max6675_read_temp(float *out)
{
	LOG_INF("MAX6675: read_temp");
	int ret;

	ret = sensor_sample_fetch_chan(dev_temp0, SENSOR_CHAN_AMBIENT_TEMP);
	if (ret < 0) {
		LOG_ERR("Could not fetch temperature (%d)", ret);
		return 0;
	}

	ret = sensor_channel_get(dev_temp0, SENSOR_CHAN_AMBIENT_TEMP, &val);
	if (ret < 0) {
		LOG_ERR("Could not get temperature (%d)", ret);
		return 0;
	}

	*out = sensor_value_to_double(&val);
	LOG_DBG("Temperature: %.2f C\n", (double)*out);

	k_sleep(K_MSEC(1000));
}

static const sensor_iface_t *iface = &(sensor_iface_t){
	.init = max6675_init,
	.read_temp = max6675_read_temp,
};

const sensor_iface_t *max6675_get_iface(void)
{
	return iface;
}
