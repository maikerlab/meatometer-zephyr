#pragma once

#define APP_TARGET_TEMP_DEFAULT_C 65.0f
#define APP_MEASURE_INTERVAL_MS 1000
#define APP_EVENT_QUEUE_DEPTH 16

#define MQTT_TOPIC_TEMP "sensors/temperature/77"
#define MQTT_TOPIC_DONE "temperature/alert"
#define MQTT_CLIENT_ID "meatometer-01"
