#pragma once

#define SENSOR_MAX_COUNT          4
#define APP_TARGET_TEMP_DEFAULT_C 65.0f
#define APP_MEASURE_INTERVAL_MS   1000
#define APP_EVENT_QUEUE_DEPTH     16

#define MQTT_CLIENT_ID              "meatometer-01"
#define MQTT_STATE_TOPIC_FMT        "meatometer/sensor/%u/temperature"
#define MQTT_AVAIL_TOPIC            "meatometer/status"
#define MQTT_DISCOVERY_PREFIX       "homeassistant"
#define MQTT_HA_STATUS_TOPIC        "homeassistant/status"
#define MQTT_TARGET_STATE_TOPIC_FMT "meatometer/sensor/%u/target"
#define MQTT_TARGET_CMD_TOPIC_FMT   "meatometer/sensor/%u/target/set"
#define MQTT_TARGET_CMD_TOPIC_SUB   "meatometer/sensor/+/target/set"
