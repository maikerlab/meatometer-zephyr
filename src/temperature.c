#include "temperature.h"
#include "app_config.h"
#include "app_events.h"
#include "sensor/sensor_registry.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor, LOG_LEVEL_DBG);

#define MEASURE_THREAD_STACK_SIZE 2048
#define MEASURE_THREAD_PRIORITY   7

/*
 * Binary semaphore: give = start measuring, take = stop measuring.
 * Initially 0 → thread sleeps until state_measuring_entry() gives.
 */
K_SEM_DEFINE(measure_sem, 0, 1);

K_THREAD_STACK_DEFINE(measure_thread_stack, MEASURE_THREAD_STACK_SIZE);

static struct k_msgq *evt_queue;
static struct k_thread measure_thread_data;

static void measure_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		/* Wait until measurement is started */
		k_sem_take(&measure_sem, K_FOREVER);
		LOG_INF("Measuring started");

		while (true) {
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

			/*
			 * Check if semaphore was given again while we were measuring.
			 * In that case, it means stop was signaled
			 * → break inner loop and wait for next start signal.
			 */
			if (k_sem_take(&measure_sem, K_MSEC(APP_MEASURE_INTERVAL_MS)) == 0) {
				LOG_INF("Measuring stopped");
				break;
			}
		}
	}
}

/* ── Public API ─────────────────────────────────────────────── */

int temperature_init(struct k_msgq *queue)
{
	LOG_INF("Initializing temperature measurement...");
	evt_queue = queue;

	k_thread_create(&measure_thread_data, measure_thread_stack,
			K_THREAD_STACK_SIZEOF(measure_thread_stack), measure_thread_fn, NULL, NULL,
			NULL, MEASURE_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&measure_thread_data, "measure_thread");

	return 0;
}

/** Start temperature measurement */
void temperature_start(void)
{
	k_sem_give(&measure_sem);
}

/** Stop temperature measurement */
void temperature_stop(void)
{
	k_sem_give(&measure_sem);
}
