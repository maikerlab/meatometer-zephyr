#include "ble_prov.h"
#include <stdio.h>
#include <string.h>
#include <zephyr/logging/log.h>

#include <net/wifi_mgmt_ext.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/net/wifi_mgmt.h>

/* STEP 2 - Include the header files for Bluetooth LE */
#include <bluetooth/services/wifi_provisioning.h>
#include <net/wifi_prov_core/wifi_prov_core.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

LOG_MODULE_REGISTER(ble_prov, LOG_LEVEL_DBG);

static struct k_msgq *evt_queue;
static bool bt_initialized;
static struct k_work_q adv_daemon_work_q;

#define ADV_DATA_UPDATE_INTERVAL 5

#define ADV_PARAM_UPDATE_DELAY 1

/* STEP 3.2 - Define indexes for accessing prov_svc_data */
#define ADV_DATA_VERSION_IDX (BT_UUID_SIZE_128 + 0)
#define ADV_DATA_FLAG_IDX (BT_UUID_SIZE_128 + 1)
#define ADV_DATA_FLAG_PROV_STATUS_BIT BIT(0)
#define ADV_DATA_FLAG_CONN_STATUS_BIT BIT(1)
#define ADV_DATA_RSSI_IDX (BT_UUID_SIZE_128 + 3)

#define PROV_BT_LE_ADV_PARAM_FAST                                              \
  BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN, BT_GAP_ADV_FAST_INT_MIN_2,               \
                  BT_GAP_ADV_FAST_INT_MAX_2, NULL)

#define PROV_BT_LE_ADV_PARAM_SLOW                                              \
  BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN, BT_GAP_ADV_SLOW_INT_MIN,                 \
                  BT_GAP_ADV_SLOW_INT_MAX, NULL)

#define ADV_DAEMON_STACK_SIZE 4096
#define ADV_DAEMON_PRIORITY 5

K_THREAD_STACK_DEFINE(adv_daemon_stack_area, ADV_DAEMON_STACK_SIZE);

/* Array for storing provisioning service data */
static uint8_t prov_svc_data[] = {BT_UUID_PROV_VAL, 0x00, 0x00, 0x00, 0x00};

/* Variable for the device name */
static uint8_t device_name[] = {'P', 'V', '0', '0', '0', '0', '0', '0'};

/* Data structure for the advertisement packet */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_PROV_VAL),
    BT_DATA(BT_DATA_NAME_COMPLETE, device_name, sizeof(device_name)),
};

/* Data structure for the scan response packet */
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_SVC_DATA128, prov_svc_data, sizeof(prov_svc_data)),
};

/* Work structures for updating advertisement parameters and data */
static struct k_work_delayable update_adv_param_work;
static struct k_work_delayable update_adv_data_work;

static void update_wifi_status_in_adv(void) {
  /* Update the firmware version*/
  prov_svc_data[ADV_DATA_VERSION_IDX] = PROV_SVC_VER;

  /* Update the provisioning state */
  if (!wifi_prov_state_get()) {
    prov_svc_data[ADV_DATA_FLAG_IDX] &= ~ADV_DATA_FLAG_PROV_STATUS_BIT;
  } else {
    prov_svc_data[ADV_DATA_FLAG_IDX] |= ADV_DATA_FLAG_PROV_STATUS_BIT;
  }

  /* Update the Wi-Fi connection status*/
  struct net_if *iface = net_if_get_first_wifi();
  struct wifi_iface_status status = {0};

  int err = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status,
                     sizeof(struct wifi_iface_status));
  if ((err != 0) || (status.state < WIFI_STATE_ASSOCIATED)) {
    prov_svc_data[ADV_DATA_FLAG_IDX] &= ~ADV_DATA_FLAG_CONN_STATUS_BIT;
    prov_svc_data[ADV_DATA_RSSI_IDX] = INT8_MIN;
  } else {
    prov_svc_data[ADV_DATA_FLAG_IDX] |= ADV_DATA_FLAG_CONN_STATUS_BIT;
    prov_svc_data[ADV_DATA_RSSI_IDX] = status.rssi;
  }
}

static void connected(struct bt_conn *conn, uint8_t err) {
  char addr[BT_ADDR_LE_STR_LEN];

  if (err) {
    LOG_ERR("BT Connection failed (err 0x%02x).\n", err);
    return;
  }

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  LOG_INF("BT Connected: %s", addr);

  /* Upon a connected event, cancel update_adv_data_work */
  k_work_cancel_delayable(&update_adv_data_work);
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  LOG_INF("BT Disconnected: %s (reason 0x%02x).\n", addr, reason);

  /* Upon a disconnected event, reschedule all work items*/
  k_work_reschedule_for_queue(&adv_daemon_work_q, &update_adv_param_work,
                              K_SECONDS(ADV_PARAM_UPDATE_DELAY));
  k_work_reschedule_for_queue(&adv_daemon_work_q, &update_adv_data_work,
                              K_NO_WAIT);
}

static void identity_resolved(struct bt_conn *conn, const bt_addr_le_t *rpa,
                              const bt_addr_le_t *identity) {
  char addr_identity[BT_ADDR_LE_STR_LEN];
  char addr_rpa[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(identity, addr_identity, sizeof(addr_identity));
  bt_addr_le_to_str(rpa, addr_rpa, sizeof(addr_rpa));

  LOG_INF("BT Identity resolved %s -> %s.\n", addr_rpa, addr_identity);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  if (!err) {
    LOG_INF("BT Security changed: %s level %u.\n", addr, level);
  } else {
    LOG_ERR("BT Security failed: %s level %u err %d.\n", addr, level, err);
  }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .identity_resolved = identity_resolved,
    .security_changed = security_changed,
};

static void auth_cancel(struct bt_conn *conn) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  LOG_INF("BT Pairing cancelled: %s", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
    .cancel = auth_cancel,
};

static void pairing_complete(struct bt_conn *conn, bool bonded) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  LOG_INF("BT pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
  LOG_INF("BT Pairing Failed (%d). Disconnecting", reason);
  bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
}

static struct bt_conn_auth_info_cb auth_info_cb_display = {

    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed,
};

static void update_adv_data_task(struct k_work *item) {
  /* Update the advertising and scan response data*/
  int err;

  update_wifi_status_in_adv();
  err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
  if (err != 0) {
    LOG_INF("Cannot update advertisement data, err = %d\n", err);
  }
  k_work_reschedule_for_queue(&adv_daemon_work_q, &update_adv_data_work,
                              K_SECONDS(ADV_DATA_UPDATE_INTERVAL));
}

static void update_adv_param_task(struct k_work *item) {
  /* Stop advertising, then start advertising again */
  int err;

  err = bt_le_adv_stop();
  if (err != 0) {
    LOG_ERR("Cannot stop advertisement: err = %d\n", err);
    return;
  }

  err = bt_le_adv_start(prov_svc_data[ADV_DATA_FLAG_IDX] &
                                ADV_DATA_FLAG_PROV_STATUS_BIT
                            ? PROV_BT_LE_ADV_PARAM_SLOW
                            : PROV_BT_LE_ADV_PARAM_FAST,
                        ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
  if (err != 0) {
    LOG_ERR("Cannot start advertisement: err = %d\n", err);
  }
}

static void byte_to_hex(char *ptr, uint8_t byte, char base) {
  int i, val;

  for (i = 0, val = (byte & 0xf0) >> 4; i < 2; i++, val = byte & 0x0f) {
    if (val < 10) {
      *ptr++ = (char)(val + '0');
    } else {
      *ptr++ = (char)(val - 10 + base);
    }
  }
}

static void update_dev_name(struct net_linkaddr *mac_addr) {
  byte_to_hex(&device_name[2], mac_addr->addr[3], 'A');
  byte_to_hex(&device_name[4], mac_addr->addr[4], 'A');
  byte_to_hex(&device_name[6], mac_addr->addr[5], 'A');
}

static int ble_prov_init(void) {
  int err;

  LOG_DBG("Initializing BLE provisioning");
  if (bt_initialized) {
    return 0;
  }

  // TODO: Enable Bluetooth and initialize BLE provisioning service
  bt_conn_auth_cb_register(&auth_cb_display);
  bt_conn_auth_info_cb_register(&auth_info_cb_display);

  err = bt_enable(NULL);
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d).\n", err);
    return 0;
  }
  LOG_INF("Bluetooth initialized.\n");

  bt_initialized = true;
  LOG_INF("BLE provisioning initialized");
  return 0;
}

static int ble_prov_start(void) {
  int err;
  LOG_DBG("Starting BLE provisioning");

  /* 1. Enable the Bluetooth Wi-Fi Provisioning Service */
  err = wifi_prov_init();
  if (err != 0) {
    LOG_ERR("Failed to initialize Wi-Fi provisioning service (err %d)", err);
    return -1;
  }
  LOG_INF("Wi-Fi provisioning service started successfully");

  /* 2. Prepare the advertisement data */
  struct net_if *iface = net_if_get_default();
  struct net_linkaddr *mac_addr = net_if_get_link_addr(iface);
  char device_name_str[sizeof(device_name) + 1];

  if (mac_addr) {
    update_dev_name(mac_addr);
  }
  device_name_str[sizeof(device_name_str) - 1] = '\0';
  memcpy(device_name_str, device_name, sizeof(device_name));
  LOG_INF("Set BT device name to %s", device_name_str);
  bt_set_name(device_name_str);

  /* 3. Start advertising */
  update_wifi_status_in_adv();

  err = bt_le_adv_start(prov_svc_data[ADV_DATA_FLAG_IDX] &
                                ADV_DATA_FLAG_PROV_STATUS_BIT
                            ? PROV_BT_LE_ADV_PARAM_SLOW
                            : PROV_BT_LE_ADV_PARAM_FAST,
                        ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
  if (err) {
    LOG_ERR("BT Advertising failed to start (err %d)", err);
    return -1;
  }
  LOG_INF("BT Advertising successfully started.");

  k_work_queue_init(&adv_daemon_work_q);
  k_work_queue_start(&adv_daemon_work_q, adv_daemon_stack_area,
                     K_THREAD_STACK_SIZEOF(adv_daemon_stack_area),
                     ADV_DAEMON_PRIORITY, NULL);

  /* 4. Initializa all work items to their respective task */
  k_work_init_delayable(&update_adv_param_work, update_adv_param_task);
  k_work_init_delayable(&update_adv_data_work, update_adv_data_task);
  k_work_schedule_for_queue(&adv_daemon_work_q, &update_adv_data_work,
                            K_SECONDS(ADV_DATA_UPDATE_INTERVAL));

  return 0;
}

static int ble_prov_stop(void) {
  LOG_DBG("Stopping BLE provisioning");
  return bt_le_adv_stop();
}

static const ble_prov_iface_t iface = {
    .init = ble_prov_init,
    .start = ble_prov_start,
    .stop = ble_prov_stop,
};

const ble_prov_iface_t *ble_prov_get_iface(struct k_msgq *msgq) {
  evt_queue = msgq;
  return &iface;
}
