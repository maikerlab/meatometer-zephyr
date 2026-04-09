#include "net_init.h"
#include "app_config.h"
#include "wifi_mgr.h"
#include "mqtt_mgr.h"

static const network_iface_t iface = {
    .wifi_connect = wifi_mgr_connect,
    .wifi_disconnect = wifi_mgr_disconnect,
    .mqtt_connect = mqtt_mgr_connect,
    .mqtt_disconnect = mqtt_mgr_disconnect,
    .mqtt_publish_temperature = mqtt_mgr_publish_temperature,
};

network_iface_t *network_init(void)
{
    return &iface;
}