#include "measure_temp.h"
#include "app_events.h"
#include "app_config.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(measure_temp, LOG_LEVEL_INF);

#define MEASURE_THREAD_STACK_SIZE 1024
#define MEASURE_THREAD_PRIORITY 7

/* ── Internal state ─────────────────────────────────────────────────── */

static const hal_iface_t *hal_ref;
static struct k_msgq *evt_queue;

/*
 * Binary semaphore: give = start measuring, take = stop measuring.
 * Initially 0 → thread sleeps until state_measuring_entry() gives.
 */
K_SEM_DEFINE(measure_sem, 0, 1);

K_THREAD_STACK_DEFINE(measure_thread_stack, MEASURE_THREAD_STACK_SIZE);
static struct k_thread measure_thread_data;

/* ── Thread function ──────────────────────────────────────────────────────────── */

static void measure_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (true)
    {
        /* Warte bis Messen gestartet wird */
        k_sem_take(&measure_sem, K_FOREVER);
        LOG_INF("Measuring started");

        while (true)
        {
            float temp;
            int ret = hal_ref->read_temp(&temp);

            if (ret == 0)
            {
                app_event_t evt = {
                    .type = EVT_TEMP_UPDATE,
                    .data.temperature = temp,
                };
                k_msgq_put(evt_queue, &evt, K_NO_WAIT);
                LOG_DBG("Posted EVT_TEMP_UPDATE: %.2f °C", (double)temp);
            }
            else
            {
                LOG_ERR("read_temp failed: %d", ret);
            }

            /*
             * Check if semaphore was given again while we were measuring.
             * In that case, it means stop was signaled 
             * → break inner loop and wait for next start signal.
             */
            if (k_sem_take(&measure_sem, K_MSEC(APP_MEASURE_INTERVAL_MS)) == 0)
            {
                LOG_INF("Measuring stopped");
                break;
            }
        }
    }
}

/** Initialize temperature measurement and starts measure_thread
 * @param hal Pointer to hardware interface
 * @param queue Pointer to app event message queue
 */
void measure_temp_init(const hal_iface_t *hal, struct k_msgq *queue)
{
    hal_ref = hal;
    evt_queue = queue;

    k_thread_create(
        &measure_thread_data,
        measure_thread_stack,
        K_THREAD_STACK_SIZEOF(measure_thread_stack),
        measure_thread_fn,
        NULL, NULL, NULL,
        MEASURE_THREAD_PRIORITY,
        0,
        K_NO_WAIT);
    k_thread_name_set(&measure_thread_data, "measure_thread");
}

/** Start temperature measurement */
void measure_temp_start(void)
{
    k_sem_give(&measure_sem);
}

/** Stop temperature measurement */
void measure_temp_stop(void)
{
    k_sem_give(&measure_sem);
}