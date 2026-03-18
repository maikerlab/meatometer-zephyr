#pragma once

#include <stdint.h>

typedef enum {
    EVT_BTN_POWER = 0,
    EVT_BTN_MEASURE,
    EVT_TEMP_REACHED,
    EVT_TEMP_UPDATE,
    EVT_WIFI_CONNECTED,
    EVT_WIFI_DISCONNECTED,
    EVT_MQTT_READY,
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    union {
        float temperature;
        int   error_code;
    } data;
} app_event_t;
