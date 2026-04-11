#include "mqtt_mgr.h"
#include "app_config.h"
#include <net/mqtt_helper.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define CONFIG_MQTT_SAMPLE_PUB_TOPIC "sensors/temperature/77"
#define CONFIG_MQTT_SAMPLE_SUB_TOPIC "sensors/temperature/+"
static struct k_msgq *evt_queue;
static bool mqtt_connected;

LOG_MODULE_REGISTER(mqtt_mgr, LOG_LEVEL_DBG);

/* Forward declarations */

/**
 * Initializes the MQTT manager.
 * Must be called before mqtt_mgr_connect().
 * @param event_queue Pointer to the app event message queue.
 */
static int mqtt_mgr_init(void);
/**
 * Connects to the MQTT broker configured in app_config.h.
 * Blocks until CONNACK is received or timeout expires.
 * On success, starts a background polling thread for keep-alive.
 * @return 0 on success, negative errno on failure.
 */
static int mqtt_mgr_connect(void);
/**
 * Disconnects from the MQTT broker and stops the polling thread.
 * @return 0 on success, negative errno on failure.
 */
static int mqtt_mgr_disconnect(void);
/**
 * Returns true if currently connected to the MQTT broker.
 */
static bool mqtt_mgr_is_connected(void);
/**
 * Publishes a temperature value to the configured MQTT topic.
 * @param temp_celsius Temperature in degrees Celsius.
 * @return 0 on success, negative errno on failure.
 */
static int mqtt_mgr_publish_temperature(float temp_celsius);

static const mqtt_iface_t mqtt_iface = {
    .init = mqtt_mgr_init,
    .connect = mqtt_mgr_connect,
    .is_connected = mqtt_mgr_is_connected,
    .disconnect = mqtt_mgr_disconnect,
    .publish_temperature = mqtt_mgr_publish_temperature,
};

// Callback functions
static void on_mqtt_connack(enum mqtt_conn_return_code return_code,
                            bool session_present) {
  if (return_code == MQTT_CONNECTION_ACCEPTED) {
    LOG_INF("Connected to MQTT broker");
    LOG_INF("Hostname: %s", CONFIG_APP_MQTT_BROKER_ADDR);
    LOG_INF("Client ID: %s", (char *)MQTT_CLIENT_ID);
    LOG_INF("Port: %d", CONFIG_MQTT_HELPER_PORT);
    LOG_INF("TLS: %s", IS_ENABLED(CONFIG_MQTT_LIB_TLS) ? "Yes" : "No");
    mqtt_connected = true;
  } else {
    LOG_WRN("Connection to broker not established, return_code: %d",
            return_code);
    mqtt_connected = false;
  }

  ARG_UNUSED(return_code);
}
static void on_mqtt_suback(uint16_t message_id, int result) {
  if (result != MQTT_SUBACK_FAILURE) {
    if (message_id == 1234) {
      LOG_INF("Subscribed to %s with QoS %d", CONFIG_MQTT_SAMPLE_SUB_TOPIC,
              result);
      return;
    }
    LOG_WRN("Subscribed to unknown topic, id: %d with QoS %d", message_id,
            result);
    return;
  }
  LOG_ERR("Topic subscription failed, error: %d", result);
}

static void on_mqtt_publish(struct mqtt_helper_buf topic,
                            struct mqtt_helper_buf payload) {
  LOG_INF("Received payload: %.*s on topic: %.*s", payload.size, payload.ptr,
          topic.size, topic.ptr);
}

static void on_mqtt_disconnect(int result) {
  mqtt_connected = false;
  LOG_INF("MQTT client disconnected: %d", result);
}

static int mqtt_mgr_init(void) {
  LOG_DBG("Initializing MQTT manager");
  mqtt_connected = false;
  struct mqtt_helper_cfg config = {
      .cb =
          {
              .on_connack = on_mqtt_connack,
              .on_disconnect = on_mqtt_disconnect,
              .on_publish = on_mqtt_publish,
              .on_suback = on_mqtt_suback,
          },
  };
  mqtt_helper_init(&config);
}

static int mqtt_mgr_connect(void) {
  LOG_INF("Connecting to MQTT broker at %s...", CONFIG_APP_MQTT_BROKER_ADDR);
  struct mqtt_helper_conn_params conn_params = {
      .hostname.ptr = CONFIG_APP_MQTT_BROKER_ADDR,
      .hostname.size = strlen(CONFIG_APP_MQTT_BROKER_ADDR),
      .device_id.ptr = MQTT_CLIENT_ID,
      .device_id.size = strlen(MQTT_CLIENT_ID),
      .user_name =
          {
              .ptr = CONFIG_APP_MQTT_USERNAME,
              .size = strlen(CONFIG_APP_MQTT_USERNAME),
          },
      .password =
          {
              .ptr = CONFIG_APP_MQTT_PASSWORD,
              .size = strlen(CONFIG_APP_MQTT_PASSWORD),
          },
  };

  int err = mqtt_helper_connect(&conn_params);
  if (err) {
    LOG_ERR("Failed to connect to MQTT, error code: %d", err);
    return -1;
  }
  LOG_INF("Connected to MQTT broker!");
  return 0;
}

static int mqtt_mgr_disconnect(void) {
  int ret = mqtt_helper_disconnect();
  if (ret != 0) {
    LOG_ERR("Failed to disconnect from MQTT, error code: %d", ret);
    return -1;
  }
  LOG_INF("Disconnected from MQTT broker!");
  return 0;
}

static int mqtt_mgr_publish_temperature(float temp_celsius) {
  if (!mqtt_connected) {
    LOG_WRN("MQTT not connected - skipping publish!");
    return -ENOTCONN;
  }

  LOG_INF("Publishing temperature: %.1f °C", temp_celsius);

  char payload[32];
  snprintk(payload, sizeof(payload), "%.1f", temp_celsius);

  struct mqtt_publish_param mqtt_param;
  mqtt_param.message.payload.data = (uint8_t *)payload;
  mqtt_param.message.payload.len = strlen(payload);
  mqtt_param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
  mqtt_param.message_id = mqtt_helper_msg_id_get(),
  mqtt_param.message.topic.topic.utf8 = CONFIG_MQTT_SAMPLE_PUB_TOPIC;
  mqtt_param.message.topic.topic.size = strlen(CONFIG_MQTT_SAMPLE_PUB_TOPIC);
  mqtt_param.dup_flag = 0;
  mqtt_param.retain_flag = 0;

  int err = mqtt_helper_publish(&mqtt_param);
  if (err) {
    LOG_WRN("Failed to send payload, err: %d", err);
    return err;
  }

  LOG_INF("Published message: \"%.*s\" on topic: \"%.*s\"",
          mqtt_param.message.payload.len, mqtt_param.message.payload.data,
          mqtt_param.message.topic.topic.size,
          mqtt_param.message.topic.topic.utf8);

  return 0;
}

static bool mqtt_mgr_is_connected(void) { return mqtt_connected; }

const mqtt_iface_t *mqtt_get_iface(struct k_msgq *msgq) {
  evt_queue = msgq;
  return &mqtt_iface;
}
