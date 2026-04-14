#include "temperature.h"
#include "app_config.h"
#include "app_events.h"
#include "sensor/sensor_registry.h"
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(sensor, LOG_LEVEL_DBG);

#define MEASURE_THREAD_STACK_SIZE 2048
#define MEASURE_THREAD_PRIORITY   7

static atomic_t measure_active = ATOMIC_INIT(0);
static struct k_condvar measure_cv;
static struct k_mutex measure_mutex;

K_THREAD_STACK_DEFINE(measure_thread_stack, MEASURE_THREAD_STACK_SIZE);

static struct k_msgq *evt_queue;
static struct k_thread measure_thread_data;

static void measure_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		/* Wait until measurement is requested */
		k_mutex_lock(&measure_mutex, K_FOREVER);
		while (!atomic_get(&measure_active)) {
			k_condvar_wait(&measure_cv, &measure_mutex, K_FOREVER);
		}
		k_mutex_unlock(&measure_mutex);

		LOG_INF("Measuring started");

		while (atomic_get(&measure_active)) {
			uint8_t mask = sensor_registry_get_connected_mask();

			for (uint8_t i = 0; i < SENSOR_MAX_COUNT; i++) {
				if (!(mask & (1U << i))) {
					continue;
				}
				const sensor_iface_t *s = sensor_registry_get(i);
				if (s == NULL) {
					continue;
				}
				float temp;
				int ret = s->read_temp(&temp);

				if (ret == 0) {
					LOG_DBG("Slot %u: %.1f °C", i, (double)temp);
					app_event_t evt = {
						.type = EVT_TEMP_UPDATE,
						.data.temp.sensor_slot = i,
						.data.temp.temperature = temp,
					};
					k_msgq_put(evt_queue, &evt, K_NO_WAIT);
				} else {
					LOG_ERR("Slot %u: read_temp failed: %d", i, ret);
				}
			}

			k_sleep(K_MSEC(APP_MEASURE_INTERVAL_MS));
		}

		LOG_INF("Measuring stopped");
	}
}

/* ── Public API ─────────────────────────────────────────────── */

int temperature_init(struct k_msgq *queue)
{
	LOG_INF("Initializing temperature measurement...");
	evt_queue = queue;

	k_mutex_init(&measure_mutex);
	k_condvar_init(&measure_cv);

	k_thread_create(&measure_thread_data, measure_thread_stack,
			K_THREAD_STACK_SIZEOF(measure_thread_stack), measure_thread_fn, NULL, NULL,
			NULL, MEASURE_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&measure_thread_data, "measure_thread");

	return 0;
}

void temperature_start(void)
{
	atomic_set(&measure_active, 1);
	k_condvar_signal(&measure_cv);
}

void temperature_stop(void)
{
	atomic_set(&measure_active, 0);
}
