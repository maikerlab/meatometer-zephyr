#pragma once

#include <stdint.h>

typedef enum {
  /* Measure button was pressed */
  EVT_BTN_MEASURE = 0,
  /* Reconnect WiFi button was pressed */
  EVT_BTN_RECONNECT_WIFI,
  /* New temperature measurement available */
  EVT_TEMP_UPDATE,
  /* Target temperature reached */
  EVT_TEMP_REACHED,
  /* WiFi connected */
  EVT_WIFI_CONNECTED,
  /* WiFi disconnected */
  EVT_WIFI_DISCONNECTED,
  /* Attempt to connect to WiFi AP failed */
  EVT_WIFI_CONNECT_FAILED,
  /* MQTT client is successfully connected to broker */
  EVT_MQTT_CONNECTED,
  /* MQTT connection to broker lost */
  EVT_MQTT_DISCONNECTED,
} app_event_type_t;

typedef struct {
  app_event_type_t type;
  union {
    float temperature;
    int error_code;
  } data;
} app_event_t;
