#include <string.h>
#include <stdio.h>
#include <zephyr/logging/log.h>
#include "ble_prov.h"

LOG_MODULE_REGISTER(ble_prov, LOG_LEVEL_DBG);

static struct k_msgq *evt_queue;
static ble_prov_done_cb_t done_cb;
static bool bt_initialized;

static int ble_prov_init(void)
{
	LOG_DBG("Initializing BLE provisioning");
	if (bt_initialized)
	{
		return 0;
	}

	// TODO: Enable Bluetooth and initialize BLE provisioning service

	bt_initialized = true;
	LOG_INF("BLE provisioning initialized");
	return 0;
}

static int ble_prov_start(ble_prov_done_cb_t cb)
{
	LOG_DBG("Starting BLE provisioning");

	// TODO: Start BLE provisioning, invoke cb when done

	return 0;
}

static int ble_prov_stop(void)
{
	LOG_DBG("Stopping BLE provisioning");

	// TODO: Stop BLE provisioning

	return 0;
}

static const ble_prov_iface_t iface = {
	.init = ble_prov_init,
	.start = ble_prov_start,
	.stop = ble_prov_stop,
};

const ble_prov_iface_t *ble_prov_get_iface(struct k_msgq *msgq)
{
	evt_queue = msgq;
	return &iface;
}
