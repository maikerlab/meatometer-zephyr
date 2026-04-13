#include "sensor_mock.h"

static float mock_temp = 25.0f;

static int mock_init(void)
{
	return 0;
}

static int mock_read_temp(float *out)
{
	*out = mock_temp;
	return 0;
}

static const sensor_iface_t mock_iface = {
	.init = mock_init,
	.read_temp = mock_read_temp,
};

const sensor_iface_t *sensor_mock_get_iface(void)
{
	return &mock_iface;
}

void sensor_mock_reset(void)
{
	mock_temp = 25.0f;
}

void sensor_mock_set_temp(float celsius)
{
	mock_temp = celsius;
}
