#include "dispatcher.h"
#include "fsm/conn_fsm.h"
#include "fsm/session_fsm.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define EVENT_THREAD_STACK_SIZE 4096
#define EVENT_THREAD_PRIORITY 5

LOG_MODULE_REGISTER(dispatcher, LOG_LEVEL_DBG);

static struct k_msgq *evt_queue;
K_THREAD_STACK_DEFINE(event_thread_stack, EVENT_THREAD_STACK_SIZE);
static struct k_thread event_thread_data;

/** Event-Thread (receives events, calls state machine)
 * @param p1 Unused
 * @param p2 Unused
 * @param p3 Unused
 */
static void dispatcher_thread_fn(void *p1, void *p2, void *p3) {
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  app_event_t evt;

  while (true) {
    k_msgq_get(evt_queue, &evt, K_FOREVER);
    LOG_DBG("Event received: %d", evt.type);
    session_fsm_handle_event(&evt);
    conn_fsm_handle_event(&evt);
  }
}

void dispatcher_init(struct k_msgq *msgq) { evt_queue = msgq; }

void dispatcher_run(void) {
  k_thread_create(&event_thread_data, event_thread_stack,
                  K_THREAD_STACK_SIZEOF(event_thread_stack),
                  dispatcher_thread_fn, NULL, NULL, NULL, EVENT_THREAD_PRIORITY,
                  0, K_NO_WAIT);
  k_thread_name_set(&event_thread_data, "dispatcher_thread");
}