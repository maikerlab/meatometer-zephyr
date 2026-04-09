#pragma once

#define APP_TARGET_TEMP_DEFAULT_C   65.0f
#define APP_MEASURE_INTERVAL_MS     500
#define APP_EVENT_QUEUE_DEPTH       16

#define MQTT_BROKER_ADDR            "192.168.1.100"
#define MQTT_BROKER_PORT            1883
#define MQTT_TOPIC_TEMP             "sensors/temperature/77"
#define MQTT_TOPIC_DONE             "temperature/alert"
#define MQTT_CLIENT_ID              "zephyr-thermo-01"
#define MQTT_USERNAME               "test"
#define MQTT_PASSWORD               "testpw"
