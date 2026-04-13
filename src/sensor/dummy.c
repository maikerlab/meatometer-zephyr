#include "dummy.h"
#include "app_config.h"
#include <math.h>
#include <stdlib.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_REGISTER(sensor_dummy, LOG_LEVEL_DBG);

#define T_START     20.0f
#define T_FINAL     85.0f
#define NOISE_RANGE 0.5f

#define DUMMY_MAX_INSTANCES SENSOR_MAX_COUNT

struct dummy_state {
	uint32_t n;
	float tau;
};

static struct dummy_state instances[DUMMY_MAX_INSTANCES];
static uint8_t instance_count;

static int sensor_init_inst(uint8_t id)
{
	instances[id].n = 0;
	instances[id].tau = 120.0f + id * 20.0f;
	return 0;
}

static int sensor_read_temp_inst(uint8_t id, float *out)
{
	struct dummy_state *s = &instances[id];
	float progress = 1.0f - expf(-(float)s->n / s->tau);
	float noise = ((float)rand() / (float)RAND_MAX - 0.5f) * 2.0f * NOISE_RANGE;

	*out = T_START + (T_FINAL - T_START) * progress + noise;
	s->n++;
	return 0;
}

/* Per-instance wrappers (C has no closures) */
#define DEFINE_DUMMY_INSTANCE(ID)                                                                  \
	static int init_##ID(void)                                                                 \
	{                                                                                          \
		return sensor_init_inst(ID);                                                       \
	}                                                                                          \
	static int read_##ID(float *out)                                                           \
	{                                                                                          \
		return sensor_read_temp_inst(ID, out);                                             \
	}                                                                                          \
	static const sensor_iface_t iface_##ID = {                                                 \
		.init = init_##ID,                                                                 \
		.read_temp = read_##ID,                                                            \
	};

DEFINE_DUMMY_INSTANCE(0)
DEFINE_DUMMY_INSTANCE(1)
DEFINE_DUMMY_INSTANCE(2)
DEFINE_DUMMY_INSTANCE(3)

static const sensor_iface_t *iface_table[] = {
	&iface_0,
	&iface_1,
	&iface_2,
	&iface_3,
};

const sensor_iface_t *sensor_dummy_get_iface(void)
{
	__ASSERT(instance_count < DUMMY_MAX_INSTANCES, "Too many dummy sensor instances");
	return iface_table[instance_count++];
}
