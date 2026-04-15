#include "mqtt_iface.h"

const mqtt_iface_t *mqtt_mock_get_iface(void);
void mqtt_mock_reset(void);
bool mqtt_mock_is_initialized(void);
bool mqtt_mock_is_connected(void);
float mqtt_mock_last_published_temp(void);
uint8_t mqtt_mock_last_published_sensor_slot(void);
uint8_t mqtt_mock_last_discovery_mask(void);
uint8_t mqtt_mock_last_target_state_slot(void);
float mqtt_mock_last_target_state_value(void);
const char *mqtt_mock_last_session_state(void);
