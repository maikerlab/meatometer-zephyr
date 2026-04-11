#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "event_handler.h"
#include "fsm/session_fsm.h"
#include "app_events.h"

LOG_MODULE_REGISTER(event_handler, LOG_LEVEL_DBG);

#define EVENT_THREAD_STACK_SIZE 2048
#define EVENT_THREAD_PRIORITY 5

static struct k_msgq *evt_queue;
static const hal_iface_t *hal_ref;

K_THREAD_STACK_DEFINE(event_thread_stack, EVENT_THREAD_STACK_SIZE);
static struct k_thread event_thread_data;

/** Button-Callback (runs in ISR context)
 * @param btn Pressed button ID
 */
static void on_button_pressed(btn_id_t btn)
{
    const app_event_t evt_map[BTN_COUNT] = {
        [BTN_MEASURE] = {.type = EVT_BTN_MEASURE},
        [BTN_RECONNECT_WIFI] = {.type = EVT_BTN_RECONNECT_WIFI},
    };

    k_msgq_put(evt_queue, &evt_map[btn], K_NO_WAIT);
}

/** Event-Thread (receives events, calls state machine)
 * @param p1 Unused
 * @param p2 Unused
 * @param p3 Unused
 */
static void event_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    app_event_t evt;

    while (true)
    {
        k_msgq_get(evt_queue, &evt, K_FOREVER);
        LOG_DBG("Event received: %d", evt.type);
        sm_handle_event(&evt);
    }
}

/** Init Event Handler
 * @param hal Pointer to hardware interface
 * @param queue Pointer to app event message queue
 */
void event_handler_init(const hal_iface_t *hal, struct k_msgq *queue)
{
    hal_ref = hal;
    evt_queue = queue;

    hal->btn_register_callback(on_button_pressed);

    k_thread_create(
        &event_thread_data,
        event_thread_stack,
        K_THREAD_STACK_SIZEOF(event_thread_stack),
        event_thread_fn,
        NULL, NULL, NULL,
        EVENT_THREAD_PRIORITY,
        0,
        K_NO_WAIT);
    k_thread_name_set(&event_thread_data, "event_thread");
}