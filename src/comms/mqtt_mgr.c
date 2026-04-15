#include "mqtt_mgr.h"
#include "app_config.h"
#include "app_events.h"
#include <net/mqtt_helper.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/app_version.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

static struct k_msgq *evt_queue;
static bool mqtt_connected;
static uint8_t discovered_mask;

LOG_MODULE_REGISTER(mqtt_mgr, LOG_LEVEL_DBG);

/* Forward declarations */

static int mqtt_mgr_init(void);
static int mqtt_mgr_connect(void);
static int mqtt_mgr_disconnect(void);
static bool mqtt_mgr_is_connected(void);
static int mqtt_mgr_publish_temperature(uint8_t sensor_slot, float temp_celsius);
static int mqtt_mgr_publish_discovery(uint8_t sensor_mask);
static int mqtt_mgr_subscribe_targets(uint8_t sensor_mask);
static int mqtt_mgr_publish_target_state(uint8_t sensor_slot, float target_celsius);
static int mqtt_mgr_publish_session_state(const char *state);

static const mqtt_iface_t mqtt_iface = {
	.init = mqtt_mgr_init,
	.connect = mqtt_mgr_connect,
	.is_connected = mqtt_mgr_is_connected,
	.disconnect = mqtt_mgr_disconnect,
	.publish_temperature = mqtt_mgr_publish_temperature,
	.publish_discovery = mqtt_mgr_publish_discovery,
	.subscribe_targets = mqtt_mgr_subscribe_targets,
	.publish_target_state = mqtt_mgr_publish_target_state,
	.publish_session_state = mqtt_mgr_publish_session_state,
};

/* ── HA discovery helpers ─────────────────────────────────────────────── */

static int publish_retained(const char *topic, const uint8_t *data, size_t len)
{
	struct mqtt_publish_param param = {
		.message.payload.data = (uint8_t *)data,
		.message.payload.len = len,
		.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE,
		.message.topic.topic.utf8 = (uint8_t *)topic,
		.message.topic.topic.size = strlen(topic),
		.message_id = mqtt_helper_msg_id_get(),
		.retain_flag = 1,
	};
	return mqtt_helper_publish(&param);
}

static int publish_availability(const char *status)
{
	return publish_retained(MQTT_AVAIL_TOPIC, (const uint8_t *)status, strlen(status));
}

static int publish_ha_config(uint8_t slot, bool first)
{
	char topic[64];
	snprintk(topic, sizeof(topic), MQTT_DISCOVERY_PREFIX "/sensor/meatometer_temp_%02u/config",
		 slot + 1);

	char payload[512];
	int len;

	if (first) {
		len = snprintk(payload, sizeof(payload),
			       "{\"name\":\"Probe %u\","
			       "\"stat_t\":\"" MQTT_STATE_TOPIC_FMT "\","
			       "\"unit_of_meas\":\"\\u00b0C\","
			       "\"dev_cla\":\"temperature\","
			       "\"stat_cla\":\"measurement\","
			       "\"uniq_id\":\"meatometer_temp_%02u\","
			       "\"avty_t\":\"" MQTT_AVAIL_TOPIC "\","
			       "\"pl_avail\":\"online\","
			       "\"pl_not_avail\":\"offline\","
			       "\"dev\":{\"ids\":[\"" MQTT_CLIENT_ID "\"],"
			       "\"name\":\"Meatometer\","
			       "\"mf\":\"Custom\","
			       "\"mdl\":\"Grill Thermometer\","
			       "\"sw\":\"%s\"}}",
			       slot + 1, slot, slot + 1, APP_VERSION_STRING);
	} else {
		len = snprintk(payload, sizeof(payload),
			       "{\"name\":\"Probe %u\","
			       "\"stat_t\":\"" MQTT_STATE_TOPIC_FMT "\","
			       "\"unit_of_meas\":\"\\u00b0C\","
			       "\"dev_cla\":\"temperature\","
			       "\"stat_cla\":\"measurement\","
			       "\"uniq_id\":\"meatometer_temp_%02u\","
			       "\"avty_t\":\"" MQTT_AVAIL_TOPIC "\","
			       "\"pl_avail\":\"online\","
			       "\"pl_not_avail\":\"offline\","
			       "\"dev\":{\"ids\":[\"" MQTT_CLIENT_ID "\"]}}",
			       slot + 1, slot, slot + 1);
	}

	LOG_INF("Publishing HA discovery for slot %u", slot);
	return publish_retained(topic, (const uint8_t *)payload, len);
}

static void subscribe_ha_status(void)
{
	static struct mqtt_topic ha_topic = {
		.topic.utf8 = MQTT_HA_STATUS_TOPIC,
		.topic.size = sizeof(MQTT_HA_STATUS_TOPIC) - 1,
		.qos = MQTT_QOS_1_AT_LEAST_ONCE,
	};
	struct mqtt_subscription_list sub_list = {
		.list = &ha_topic,
		.list_count = 1,
		.message_id = mqtt_helper_msg_id_get(),
	};
	int err = mqtt_helper_subscribe(&sub_list);
	if (err) {
		LOG_WRN("Failed to subscribe to %s: %d", MQTT_HA_STATUS_TOPIC, err);
	} else {
		LOG_INF("Subscribed to %s", MQTT_HA_STATUS_TOPIC);
	}
}

static int publish_ha_session_state_config(bool first)
{
	char topic[64];
	snprintk(topic, sizeof(topic), MQTT_DISCOVERY_PREFIX "/sensor/meatometer_session/config");

	char payload[512];
	int len;

	if (first) {
		len = snprintk(payload, sizeof(payload),
			       "{\"name\":\"Session State\","
			       "\"stat_t\":\"" MQTT_SESSION_STATE_TOPIC "\","
			       "\"ic\":\"mdi:state-machine\","
			       "\"uniq_id\":\"meatometer_session\","
			       "\"avty_t\":\"" MQTT_AVAIL_TOPIC "\","
			       "\"pl_avail\":\"online\","
			       "\"pl_not_avail\":\"offline\","
			       "\"dev\":{\"ids\":[\"" MQTT_CLIENT_ID "\"],"
			       "\"name\":\"Meatometer\","
			       "\"mf\":\"Custom\","
			       "\"mdl\":\"Grill Thermometer\","
			       "\"sw\":\"%s\"}}",
			       APP_VERSION_STRING);
	} else {
		len = snprintk(payload, sizeof(payload),
			       "{\"name\":\"Session State\","
			       "\"stat_t\":\"" MQTT_SESSION_STATE_TOPIC "\","
			       "\"ic\":\"mdi:state-machine\","
			       "\"uniq_id\":\"meatometer_session\","
			       "\"avty_t\":\"" MQTT_AVAIL_TOPIC "\","
			       "\"pl_avail\":\"online\","
			       "\"pl_not_avail\":\"offline\","
			       "\"dev\":{\"ids\":[\"" MQTT_CLIENT_ID "\"]}}");
	}

	LOG_INF("Publishing HA session state discovery");
	return publish_retained(topic, (const uint8_t *)payload, len);
}

/* ── Callback functions ──────────────────────────────────────────────── */

static void on_mqtt_connack(enum mqtt_conn_return_code return_code, bool session_present)
{
	if (return_code == MQTT_CONNECTION_ACCEPTED) {
		LOG_INF("Connected to MQTT broker");
		LOG_INF("Hostname: %s", CONFIG_APP_MQTT_BROKER_ADDR);
		LOG_INF("Client ID: %s", (char *)MQTT_CLIENT_ID);
		LOG_INF("Port: %d", CONFIG_MQTT_HELPER_PORT);
		LOG_INF("TLS: %s", IS_ENABLED(CONFIG_MQTT_LIB_TLS) ? "Yes" : "No");
		mqtt_connected = true;

		subscribe_ha_status();

		app_event_t ready_evt = {.type = EVT_MQTT_CONNECTED};
		k_msgq_put(evt_queue, &ready_evt, K_NO_WAIT);
	} else {
		LOG_WRN("Connection to broker not established, return_code: %d", return_code);
		mqtt_connected = false;
	}
}

static void on_mqtt_suback(uint16_t message_id, int result)
{
	if (result != MQTT_SUBACK_FAILURE) {
		LOG_INF("Subscription acknowledged, id: %d, QoS: %d", message_id, result);
		return;
	}
	LOG_ERR("Topic subscription failed, error: %d", result);
}

#define TARGET_CMD_PREFIX     "meatometer/sensor/"
#define TARGET_CMD_PREFIX_LEN (sizeof(TARGET_CMD_PREFIX) - 1)
#define TARGET_CMD_SUFFIX     "/target/set"
#define TARGET_CMD_SUFFIX_LEN (sizeof(TARGET_CMD_SUFFIX) - 1)

static bool parse_target_cmd_topic(const char *topic_ptr, size_t topic_len, uint8_t *slot_out)
{
	if (topic_len < TARGET_CMD_PREFIX_LEN + 1 + TARGET_CMD_SUFFIX_LEN) {
		return false;
	}
	if (memcmp(topic_ptr, TARGET_CMD_PREFIX, TARGET_CMD_PREFIX_LEN) != 0) {
		return false;
	}
	const char *suffix = topic_ptr + topic_len - TARGET_CMD_SUFFIX_LEN;
	if (memcmp(suffix, TARGET_CMD_SUFFIX, TARGET_CMD_SUFFIX_LEN) != 0) {
		return false;
	}
	/* Extract slot number between prefix and suffix */
	size_t digit_len = topic_len - TARGET_CMD_PREFIX_LEN - TARGET_CMD_SUFFIX_LEN;
	if (digit_len == 0 || digit_len > 2) {
		return false;
	}
	unsigned int slot = 0;
	for (size_t i = 0; i < digit_len; i++) {
		char ch = topic_ptr[TARGET_CMD_PREFIX_LEN + i];
		if (ch < '0' || ch > '9') {
			return false;
		}
		slot = slot * 10 + (ch - '0');
	}
	if (slot >= SENSOR_MAX_COUNT) {
		return false;
	}
	*slot_out = (uint8_t)slot;
	return true;
}

static void on_mqtt_publish(struct mqtt_helper_buf topic, struct mqtt_helper_buf payload)
{
	LOG_INF("Received payload: %.*s on topic: %.*s", payload.size, payload.ptr, topic.size,
		topic.ptr);

	/* Handle HA restart */
	if (topic.size == (sizeof(MQTT_HA_STATUS_TOPIC) - 1) &&
	    memcmp(topic.ptr, MQTT_HA_STATUS_TOPIC, topic.size) == 0 && payload.size >= 6 &&
	    memcmp(payload.ptr, "online", 6) == 0 && discovered_mask != 0) {
		LOG_INF("Home Assistant restarted, re-publishing discovery");
		mqtt_mgr_publish_discovery(discovered_mask);
		mqtt_mgr_subscribe_targets(discovered_mask);
		return;
	}

	/* Handle target temperature command */
	uint8_t slot;
	if (parse_target_cmd_topic(topic.ptr, topic.size, &slot)) {
		char buf[8];
		size_t copy_len = MIN(payload.size, sizeof(buf) - 1);
		memcpy(buf, payload.ptr, copy_len);
		buf[copy_len] = '\0';

		char *endptr;
		float target = strtof(buf, &endptr);
		if (endptr == buf) {
			LOG_WRN("Invalid target payload for slot %u: %.*s", slot, payload.size,
				payload.ptr);
			return;
		}

		LOG_INF("Target temperature for slot %u set to %.1f °C", slot, (double)target);
		app_event_t evt = {
			.type = EVT_TARGET_TEMP_SET,
			.data.target.sensor_slot = slot,
			.data.target.temperature = target,
		};
		k_msgq_put(evt_queue, &evt, K_NO_WAIT);
	}
}

static void on_mqtt_disconnect(int result)
{
	mqtt_connected = false;
	LOG_INF("MQTT client disconnected: %d", result);
	app_event_t disc_evt = {.type = EVT_MQTT_DISCONNECTED};
	k_msgq_put(evt_queue, &disc_evt, K_NO_WAIT);
}

static int mqtt_mgr_init(void)
{
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
	return mqtt_helper_init(&config);
}

static int mqtt_mgr_connect(void)
{
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
	return 0;
}

static int mqtt_mgr_disconnect(void)
{
	int ret;
	ret = publish_availability("offline");
	if (ret != 0) {
		LOG_ERR("Failed to publish availability: %d", ret);
		return -1;
	}

	ret = mqtt_helper_disconnect();
	if (ret != 0) {
		LOG_ERR("Failed to disconnect from MQTT, error code: %d", ret);
		return -1;
	}
	LOG_INF("Disconnected from MQTT broker!");
	return 0;
}

static int mqtt_mgr_publish_temperature(uint8_t sensor_slot, float temp_celsius)
{
	if (!mqtt_connected) {
		LOG_WRN("MQTT not connected - skipping publish!");
		return -ENOTCONN;
	}

	char payload[8];
	snprintk(payload, sizeof(payload), "%.1f", (double)temp_celsius);

	char topic[48];
	snprintk(topic, sizeof(topic), MQTT_STATE_TOPIC_FMT, sensor_slot);

	struct mqtt_publish_param mqtt_param = {
		.message.payload.data = (uint8_t *)payload,
		.message.payload.len = strlen(payload),
		.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE,
		.message.topic.topic.utf8 = (uint8_t *)topic,
		.message.topic.topic.size = strlen(topic),
		.message_id = mqtt_helper_msg_id_get(),
	};

	int err = mqtt_helper_publish(&mqtt_param);
	if (err) {
		LOG_WRN("Failed to send payload, err: %d", err);
		return err;
	}

	LOG_INF("Published temperature: \"%.*s\" on topic: \"%.*s\"",
		mqtt_param.message.payload.len, mqtt_param.message.payload.data,
		mqtt_param.message.topic.topic.size, mqtt_param.message.topic.topic.utf8);

	return 0;
}

static int mqtt_mgr_publish_discovery(uint8_t sensor_mask)
{
	if (!mqtt_connected) {
		LOG_WRN("MQTT not connected - skipping discovery publish");
		return -ENOTCONN;
	}
	LOG_DBG("Publishing HA discovery for sensor mask 0x%02x", sensor_mask);

	/* Session state entity is always published (first, with full device info) */
	int err = publish_ha_session_state_config(true);
	if (err) {
		LOG_ERR("Failed to publish HA session state config: %d", err);
		return err;
	}

	for (uint8_t i = 0; i < SENSOR_MAX_COUNT; i++) {
		if (!(sensor_mask & (1U << i))) {
			continue;
		}
		err = publish_ha_config(i, false);
		if (err) {
			LOG_ERR("Failed to publish HA config for slot %u: %d", i, err);
			return err;
		}
	}

	discovered_mask = sensor_mask;

	err = publish_availability("online");
	if (err) {
		LOG_ERR("Failed to publish availability: %d", err);
		return err;
	}

	LOG_INF("Published HA discovery for mask 0x%02x", sensor_mask);
	return 0;
}

/* ── Target temperature helpers ───────────────────────────────────────── */

static int publish_ha_number_config(uint8_t slot, bool first)
{
	char topic[64];
	snprintk(topic, sizeof(topic),
		 MQTT_DISCOVERY_PREFIX "/number/meatometer_target_%02u/config", slot + 1);

	char payload[512];
	int len;

	if (first) {
		len = snprintk(payload, sizeof(payload),
			       "{\"name\":\"Probe %u Target\","
			       "\"stat_t\":\"" MQTT_TARGET_STATE_TOPIC_FMT "\","
			       "\"cmd_t\":\"" MQTT_TARGET_CMD_TOPIC_FMT "\","
			       "\"min\":30,\"max\":300,\"step\":1,"
			       "\"unit_of_meas\":\"\\u00b0C\","
			       "\"dev_cla\":\"temperature\","
			       "\"uniq_id\":\"meatometer_target_%02u\","
			       "\"avty_t\":\"" MQTT_AVAIL_TOPIC "\","
			       "\"pl_avail\":\"online\","
			       "\"pl_not_avail\":\"offline\","
			       "\"dev\":{\"ids\":[\"" MQTT_CLIENT_ID "\"],"
			       "\"name\":\"Meatometer\","
			       "\"mf\":\"Custom\","
			       "\"mdl\":\"Grill Thermometer\","
			       "\"sw\":\"%s\"}}",
			       slot + 1, slot, slot, slot + 1, APP_VERSION_STRING);
	} else {
		len = snprintk(payload, sizeof(payload),
			       "{\"name\":\"Probe %u Target\","
			       "\"stat_t\":\"" MQTT_TARGET_STATE_TOPIC_FMT "\","
			       "\"cmd_t\":\"" MQTT_TARGET_CMD_TOPIC_FMT "\","
			       "\"min\":30,\"max\":300,\"step\":1,"
			       "\"unit_of_meas\":\"\\u00b0C\","
			       "\"dev_cla\":\"temperature\","
			       "\"uniq_id\":\"meatometer_target_%02u\","
			       "\"avty_t\":\"" MQTT_AVAIL_TOPIC "\","
			       "\"pl_avail\":\"online\","
			       "\"pl_not_avail\":\"offline\","
			       "\"dev\":{\"ids\":[\"" MQTT_CLIENT_ID "\"]}}",
			       slot + 1, slot, slot, slot + 1);
	}

	LOG_INF("Publishing HA number discovery for slot %u", slot);
	return publish_retained(topic, (const uint8_t *)payload, len);
}

static void subscribe_target_cmd(void)
{
	static struct mqtt_topic target_topic = {
		.topic.utf8 = MQTT_TARGET_CMD_TOPIC_SUB,
		.topic.size = sizeof(MQTT_TARGET_CMD_TOPIC_SUB) - 1,
		.qos = MQTT_QOS_1_AT_LEAST_ONCE,
	};
	struct mqtt_subscription_list sub_list = {
		.list = &target_topic,
		.list_count = 1,
		.message_id = mqtt_helper_msg_id_get(),
	};
	int err = mqtt_helper_subscribe(&sub_list);
	if (err) {
		LOG_WRN("Failed to subscribe to %s: %d", MQTT_TARGET_CMD_TOPIC_SUB, err);
	} else {
		LOG_INF("Subscribed to %s", MQTT_TARGET_CMD_TOPIC_SUB);
	}
}

static int mqtt_mgr_subscribe_targets(uint8_t sensor_mask)
{
	if (!mqtt_connected) {
		LOG_WRN("MQTT not connected - skipping target subscription");
		return -ENOTCONN;
	}

	subscribe_target_cmd();

	bool first = true;
	for (uint8_t i = 0; i < SENSOR_MAX_COUNT; i++) {
		if (!(sensor_mask & (1U << i))) {
			continue;
		}
		int err = publish_ha_number_config(i, first);
		if (err) {
			LOG_ERR("Failed to publish HA number config for slot %u: %d", i, err);
			return err;
		}
		first = false;
	}

	LOG_INF("Published HA number discovery for mask 0x%02x", sensor_mask);
	return 0;
}

static int mqtt_mgr_publish_target_state(uint8_t sensor_slot, float target_celsius)
{
	if (!mqtt_connected) {
		LOG_WRN("MQTT not connected - skipping target state publish");
		return -ENOTCONN;
	}

	char payload[8];
	snprintk(payload, sizeof(payload), "%.1f", (double)target_celsius);

	char topic[48];
	snprintk(topic, sizeof(topic), MQTT_TARGET_STATE_TOPIC_FMT, sensor_slot);

	return publish_retained(topic, (const uint8_t *)payload, strlen(payload));
}

static int mqtt_mgr_publish_session_state(const char *state)
{
	if (!mqtt_connected) {
		LOG_WRN("MQTT not connected - skipping session state publish");
		return -ENOTCONN;
	}

	return publish_retained(MQTT_SESSION_STATE_TOPIC, (const uint8_t *)state, strlen(state));
}

static bool mqtt_mgr_is_connected(void)
{
	return mqtt_connected;
}

const mqtt_iface_t *mqtt_get_iface(struct k_msgq *msgq)
{
	evt_queue = msgq;
	return &mqtt_iface;
}
