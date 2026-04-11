#include "dummy.h"
#include <stdlib.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_dummy, LOG_LEVEL_DBG);

// static const struct device *const dev_temp0 = DEVICE_DT_GET(DT_ALIAS(temp0));
static float last_temp = 0.0f;
static float min = 20.0f;
static float max = 80.0f;

static int sensor_init(void) {
  // Nothing to initialize for dummy sensor
  return 0;
}

static int sensor_read_temp(float *out) {
  LOG_DBG("Reading temperature from sensor...");

  // Publish random temperature values for testing
  float r = (float)rand() / (float)RAND_MAX;
  last_temp = min + r * (max - min);
  *out = last_temp;
  return 0;
}

static const sensor_iface_t iface = {
    .init = sensor_init,
    .read_temp = sensor_read_temp,
};

const sensor_iface_t *sensor_dummy_get_iface(void) { return &iface; }
