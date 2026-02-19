#include "temp.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

int temp_read(const struct device *dev, double *val)
{
    int ret;

    ret = sensor_sample_fetch_chan(dev, SENSOR_CHAN_AMBIENT_TEMP);
    if (ret < 0)
    {
        printf("Could not fetch temperature: %d\n", ret);
        return ret;
    }

    struct sensor_value value;
    ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &value);
    if (ret < 0)
    {
        printf("Could not get temperature: %d\n", ret);
    }
    // TODO: comment in if sensor is available and ready to use via SPI
    //*val = sensor_value_to_double(&value);
    *val = 23.5;
    return ret;
}