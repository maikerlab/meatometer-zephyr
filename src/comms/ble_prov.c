/*
 * Copyright (c) 2026 Maik Lorenz <maik.lorenz@pm.me>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file ble_prov.c
 * @brief BLE WiFi Provisioning — production implementation
 *
 * Uses the NCS WiFi Provisioning library to let a phone app (e.g. nRF WiFi
 * Provisioner) deliver WiFi credentials to the device over Bluetooth LE.
 *
 * Architecture:
 *   wifi_prov_core   — transport-agnostic provisioning logic (protobuf
 *                      messages, credential storage, state machine)
 *   wifi_prov_ble    — BLE transport layer; registers a GATT service with
 *                      three characteristics:
 *                        - Information       (read)           — protocol
 * version
 *                        - Operation Control (write/indicate) — client requests
 *                        - Data Out          (notify)         — async results
 *
 * Provisioning flow:
 *   1. ble_prov_init()  — enables the Bluetooth stack, registers auth callbacks
 *   2. ble_prov_start() — registers the GATT provisioning service, derives a
 *                         unique BLE device name from the WiFi MAC address,
 *                         starts BLE advertising, and launches a background
 *                         work queue that periodically refreshes the scan
 *                         response with WiFi status / RSSI
 *   3. Phone connects, discovers the service, writes WiFi credentials via a
 *      protobuf-encoded SET_CONFIG message to the Operation Control Point
 *   4. The NCS library stores credentials (Zephyr WiFi Credentials / NVS) and
 *      triggers WiFi connection internally
 *   5. wifi_mgr.c's net_mgmt callback posts EVT_WIFI_CONNECTED to the FSM
 *
 * TODO: ble_prov_start() is not idempotent — calling it twice will re-init the
 *       work queue and call wifi_prov_init() again. Add a guard flag or
 *       cleanup logic before re-initializing.
 *
 * TODO: ble_prov_stop() only stops advertising but does not cancel the daemon
 *       work queue tasks. The periodic tasks will keep firing and fail because
 *       no advertiser is active. Cancel work items in ble_prov_stop().
 */

#include "ble_prov.h"
#include <stdio.h>
#include <string.h>
#include <zephyr/logging/log.h>

#include <net/wifi_mgmt_ext.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/net/wifi_mgmt.h>

#include <bluetooth/services/wifi_provisioning.h>
#include <net/wifi_prov_core/wifi_prov_core.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

LOG_MODULE_REGISTER(ble_prov, LOG_LEVEL_DBG);

/* ── Module state ────────────────────────────────────────────────────── */

static bool bt_initialized;

/* ── Advertising timing ──────────────────────────────────────────────── */

/*
 * How often (in seconds) the daemon work queue refreshes the scan response
 * data (WiFi provisioning/connection status and RSSI).
 */
#define ADV_DATA_UPDATE_INTERVAL 5

/*
 * Delay (in seconds) before switching advertising parameters after a BLE
 * client disconnects.  Gives the stack time to settle before restarting.
 */
#define ADV_PARAM_UPDATE_DELAY 1

/* ── Scan-response service data layout ───────────────────────────────
 *
 * prov_svc_data[] is included in the scan response as BT_DATA_SVC_DATA128.
 * The first 16 bytes are the 128-bit provisioning service UUID, followed by
 * 4 application-specific bytes:
 *
 *   Byte offset (after UUID)   Content
 *   ─────────────────────────  ──────────────────────────────────────
 *   +0  (VERSION)              Protocol version (PROV_SVC_VER = 0x01)
 *   +1  (FLAGS LSB)            Bit 0: provisioned (creds stored)
 *                              Bit 1: WiFi connected
 *   +2  (FLAGS MSB)            Reserved
 *   +3  (RSSI)                 WiFi RSSI (signed int8); INT8_MIN if offline
 */
#define ADV_DATA_VERSION_IDX (BT_UUID_SIZE_128 + 0)
#define ADV_DATA_FLAG_IDX (BT_UUID_SIZE_128 + 1)
#define ADV_DATA_FLAG_PROV_STATUS_BIT BIT(0)
#define ADV_DATA_FLAG_CONN_STATUS_BIT BIT(1)
#define ADV_DATA_RSSI_IDX (BT_UUID_SIZE_128 + 3)

/* ── Advertising parameters ──────────────────────────────────────────
 *
 * FAST: ~100 ms interval — used when the device is NOT provisioned so that
 *       a phone scanning nearby discovers it quickly.
 * SLOW: ~1 s interval — used after provisioning to reduce power consumption
 *       while still allowing re-provisioning if needed.
 */
#define PROV_BT_LE_ADV_PARAM_FAST                                              \
  BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN, BT_GAP_ADV_FAST_INT_MIN_2,               \
                  BT_GAP_ADV_FAST_INT_MAX_2, NULL)

#define PROV_BT_LE_ADV_PARAM_SLOW                                              \
  BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN, BT_GAP_ADV_SLOW_INT_MIN,                 \
                  BT_GAP_ADV_SLOW_INT_MAX, NULL)

/* ── Daemon work queue ───────────────────────────────────────────────
 *
 * A dedicated work queue runs two periodic tasks while advertising:
 *   - update_adv_data_work  : refreshes scan response (WiFi status / RSSI)
 *   - update_adv_param_work : restarts advertising with updated fast/slow
 *                             interval after a BLE disconnect
 */
#define ADV_DAEMON_STACK_SIZE 4096
#define ADV_DAEMON_PRIORITY 5

K_THREAD_STACK_DEFINE(adv_daemon_stack_area, ADV_DAEMON_STACK_SIZE);
static struct k_work_q adv_daemon_work_q;
static struct k_work_delayable update_adv_param_work;
static struct k_work_delayable update_adv_data_work;

/* ── Advertising & scan-response payloads ────────────────────────────
 *
 * prov_svc_data[]: 128-bit service UUID (BT_UUID_PROV_VAL) followed by
 *                  4 data bytes (version, flags, reserved, RSSI).
 *                  Included in the scan response so a phone can read
 *                  provisioning/connection status without connecting.
 *
 * device_name[]:   BLE device name — "PV" prefix + 6 uppercase hex digits
 *                  derived from the last 3 bytes of the WiFi MAC address.
 *                  Example: MAC ...1A:2B:3C → "PV1A2B3C"
 *
 * ad[]: Advertising packet — BLE flags, service UUID, and device name.
 *       This is what a scanner sees in the initial advertisement.
 *
 * sd[]: Scan response packet — service data with WiFi status.
 *       Returned when a scanner sends a scan request.
 */
static uint8_t prov_svc_data[] = {BT_UUID_PROV_VAL, 0x00, 0x00, 0x00, 0x00};

static uint8_t device_name[] = {'P', 'V', '0', '0', '0', '0', '0', '0'};

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_PROV_VAL),
    BT_DATA(BT_DATA_NAME_COMPLETE, device_name, sizeof(device_name)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_SVC_DATA128, prov_svc_data, sizeof(prov_svc_data)),
};

/* ── Scan-response data refresh ──────────────────────────────────────
 *
 * Updates the 4 data bytes in prov_svc_data[] with current state:
 *   - Protocol version (PROV_SVC_VER)
 *   - Provisioning status bit (are credentials stored?)
 *   - WiFi connection status bit + RSSI
 *
 * Called periodically by update_adv_data_task() and once at startup
 * before the first bt_le_adv_start().
 */
static void update_wifi_status_in_adv(void) {
  prov_svc_data[ADV_DATA_VERSION_IDX] = PROV_SVC_VER;

  /* Bit 0: provisioning status — set if wifi_prov_core has stored creds */
  if (!wifi_prov_state_get()) {
    prov_svc_data[ADV_DATA_FLAG_IDX] &= ~ADV_DATA_FLAG_PROV_STATUS_BIT;
  } else {
    prov_svc_data[ADV_DATA_FLAG_IDX] |= ADV_DATA_FLAG_PROV_STATUS_BIT;
  }

  /* Bit 1: WiFi connection status + RSSI from the WiFi driver */
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

/* ── BLE connection callbacks ────────────────────────────────────────
 *
 * Registered via BT_CONN_CB_DEFINE (compile-time callback registration).
 * These fire for every BLE connection/disconnection on this device.
 */

/**
 * Called when a BLE client (phone) connects.
 * Cancels the periodic scan-response update because a connected client
 * can read status directly via the GATT Information characteristic.
 */
static void connected(struct bt_conn *conn, uint8_t err) {
  char addr[BT_ADDR_LE_STR_LEN];

  if (err) {
    LOG_ERR("BT Connection failed (err 0x%02x)", err);
    return;
  }

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  LOG_INF("BT Connected: %s", addr);

  /* No need to update scan-response while a client is connected */
  k_work_cancel_delayable(&update_adv_data_work);
}

/**
 * Called when a BLE client disconnects.
 * Reschedules both daemon tasks so advertising resumes with fresh data:
 *   - update_adv_param_work: restarts advertising with the appropriate
 *     fast/slow interval (after a 1 s delay to let the stack settle)
 *   - update_adv_data_work: immediately refreshes scan-response data
 */
static void disconnected(struct bt_conn *conn, uint8_t reason) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  LOG_INF("BT Disconnected: %s (reason 0x%02x)", addr, reason);

  k_work_reschedule_for_queue(&adv_daemon_work_q, &update_adv_param_work,
                              K_SECONDS(ADV_PARAM_UPDATE_DELAY));
  k_work_reschedule_for_queue(&adv_daemon_work_q, &update_adv_data_work,
                              K_NO_WAIT);
}

/** Logged when a peer's resolvable private address is matched to an identity.
 */
static void identity_resolved(struct bt_conn *conn, const bt_addr_le_t *rpa,
                              const bt_addr_le_t *identity) {
  char addr_identity[BT_ADDR_LE_STR_LEN];
  char addr_rpa[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(identity, addr_identity, sizeof(addr_identity));
  bt_addr_le_to_str(rpa, addr_rpa, sizeof(addr_rpa));

  LOG_INF("BT Identity resolved %s -> %s", addr_rpa, addr_identity);
}

/** Logged when the security level of a connection changes (e.g. after pairing).
 */
static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  if (!err) {
    LOG_INF("BT Security changed: %s level %u", addr, level);
  } else {
    LOG_ERR("BT Security failed: %s level %u err %d", addr, level, err);
  }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .identity_resolved = identity_resolved,
    .security_changed = security_changed,
};

/* ── Pairing / authentication callbacks ──────────────────────────────
 *
 * auth_cb_display:      handles pairing cancellation (e.g. user aborts)
 * auth_info_cb_display: informational — logs success/failure of pairing.
 *                       On failure, the connection is dropped immediately.
 *
 * The provisioning GATT characteristics require encryption, so a pairing
 * exchange happens before the phone can write WiFi credentials.
 */

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
  LOG_INF("BT pairing completed: %s, bonded: %d", addr, bonded);
}

/** On pairing failure, disconnect the client to prevent unauthenticated access.
 */
static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
  LOG_INF("BT Pairing Failed (%d). Disconnecting", reason);
  bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
}

static struct bt_conn_auth_info_cb auth_info_cb_display = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed,
};

/* ── Daemon work-queue tasks ─────────────────────────────────────────*/

/**
 * Periodic task: refreshes scan-response data with current WiFi status and
 * RSSI, then reschedules itself every ADV_DATA_UPDATE_INTERVAL seconds.
 * This lets a scanning phone see the device's connectivity status without
 * having to connect.
 */
static void update_adv_data_task(struct k_work *item) {
  update_wifi_status_in_adv();
  int err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
  if (err != 0) {
    LOG_INF("Cannot update advertisement data, err = %d", err);
  }
  k_work_reschedule_for_queue(&adv_daemon_work_q, &update_adv_data_work,
                              K_SECONDS(ADV_DATA_UPDATE_INTERVAL));
}

/**
 * One-shot task: stops and restarts advertising to apply new advertising
 * parameters.  After provisioning, the interval switches from fast (~100 ms)
 * to slow (~1 s).  BLE advertising parameters cannot be changed on-the-fly,
 * so a stop/start cycle is required.
 *
 * Typically scheduled after a BLE client disconnects (see disconnected()).
 */
static void update_adv_param_task(struct k_work *item) {
  int err;

  err = bt_le_adv_stop();
  if (err != 0) {
    LOG_ERR("Cannot stop advertisement: err = %d", err);
    return;
  }

  /* Pick fast or slow interval based on current provisioning state */
  err = bt_le_adv_start(prov_svc_data[ADV_DATA_FLAG_IDX] &
                                ADV_DATA_FLAG_PROV_STATUS_BIT
                            ? PROV_BT_LE_ADV_PARAM_SLOW
                            : PROV_BT_LE_ADV_PARAM_FAST,
                        ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
  if (err != 0) {
    LOG_ERR("Cannot start advertisement: err = %d", err);
  }
}

/* ── Device name derivation ──────────────────────────────────────────*/

/**
 * Convert a single byte to two uppercase hex characters.
 * @param ptr   Output pointer (must have room for 2 chars)
 * @param byte  Value to convert
 * @param base  Base character for digits A-F (pass 'A' for uppercase)
 */
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

/**
 * Derive a unique BLE device name from the WiFi MAC address.
 * Takes the last 3 octets of the MAC and writes them as uppercase hex
 * into device_name[2..7], producing names like "PV1A2B3C".
 */
static void update_dev_name(struct net_linkaddr *mac_addr) {
  byte_to_hex(&device_name[2], mac_addr->addr[3], 'A');
  byte_to_hex(&device_name[4], mac_addr->addr[4], 'A');
  byte_to_hex(&device_name[6], mac_addr->addr[5], 'A');
}

/* ── ble_prov_iface_t implementation ─────────────────────────────────*/

/**
 * Initialize the Bluetooth stack and register authentication callbacks.
 *
 * Auth callbacks are registered BEFORE bt_enable() because the stack may
 * invoke them during initialization if a bonded peer reconnects immediately.
 *
 * Guarded against double-init: subsequent calls are no-ops.
 *
 * @return 0 on success, negative errno on failure
 */
static int ble_prov_init(void) {
  LOG_DBG("Initializing BLE provisioning");
  if (bt_initialized) {
    return 0;
  }

  bt_conn_auth_cb_register(&auth_cb_display);
  bt_conn_auth_info_cb_register(&auth_info_cb_display);

  int err = bt_enable(NULL);
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d)", err);
    return err;
  }
  LOG_INF("Bluetooth initialized");

  bt_initialized = true;
  return 0;
}

/**
 * Start BLE WiFi provisioning.
 *
 * Sequence:
 *   1. Register the WiFi Provisioning GATT service (wifi_prov_init).
 *      This adds the three characteristics that the phone app uses to
 *      send WiFi credentials via protobuf-encoded messages.
 *
 *   2. Derive a unique BLE device name from the WiFi interface MAC address
 *      so that multiple Meatometer devices are distinguishable.
 *
 *   3. Populate the scan-response data with current WiFi status and start
 *      BLE advertising.  Unprovisioned devices use a fast advertising
 *      interval for quick discovery; provisioned devices use slow.
 *
 *   4. Start a daemon work queue with two periodic tasks:
 *      - update_adv_data_work:  refreshes WiFi status in scan response
 *                                every ADV_DATA_UPDATE_INTERVAL seconds
 *      - update_adv_param_work: scheduled on BLE disconnect to restart
 *                                advertising with the right interval
 *
 * @return 0 on success, -1 on failure
 */
static int ble_prov_start(void) {
  int err;
  LOG_DBG("Starting BLE provisioning");

  /* 1. Register the WiFi Provisioning GATT service */
  err = wifi_prov_init();
  if (err != 0) {
    LOG_ERR("Failed to initialize Wi-Fi provisioning service (err %d)", err);
    return -1;
  }
  LOG_INF("Wi-Fi provisioning service started successfully");

  /* 2. Derive a unique device name from the WiFi MAC address */
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

  /* 3. Start BLE advertising with current WiFi status in scan response */
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
  LOG_INF("BT Advertising successfully started");

  /* 4. Launch daemon work queue for periodic advertisement updates */
  k_work_queue_init(&adv_daemon_work_q);
  k_work_queue_start(&adv_daemon_work_q, adv_daemon_stack_area,
                     K_THREAD_STACK_SIZEOF(adv_daemon_stack_area),
                     ADV_DAEMON_PRIORITY, NULL);

  k_work_init_delayable(&update_adv_param_work, update_adv_param_task);
  k_work_init_delayable(&update_adv_data_work, update_adv_data_task);
  k_work_schedule_for_queue(&adv_daemon_work_q, &update_adv_data_work,
                            K_SECONDS(ADV_DATA_UPDATE_INTERVAL));

  return 0;
}

/**
 * Stop BLE advertising.
 *
 * Note: this does NOT deinit the Bluetooth stack or unregister the GATT
 * service — only stops advertising so the device is no longer discoverable.
 * The GATT service remains registered for potential future restarts.
 *
 * TODO: Cancel update_adv_data_work and update_adv_param_work here to
 *       prevent the daemon tasks from firing against a stopped advertiser.
 *
 * @return 0 on success, negative errno on failure
 */
static int ble_prov_stop(void) {
  LOG_DBG("Stopping BLE provisioning");
  return bt_le_adv_stop();
}

/* ── Interface vtable ────────────────────────────────────────────────── */

static const ble_prov_iface_t iface = {
    .init = ble_prov_init,
    .start = ble_prov_start,
    .stop = ble_prov_stop,
};

const ble_prov_iface_t *ble_prov_get_iface(struct k_msgq *msgq) {
  /* evt_queue is currently unused — provisioning state changes are
   * communicated indirectly via wifi_mgr's EVT_WIFI_CONNECTED event
   * rather than a dedicated provisioning event. */
  (void)msgq;
  return &iface;
}
