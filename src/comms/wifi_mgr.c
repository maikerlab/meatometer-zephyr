#include "wifi_mgr.h"
#include "app_events.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/net/wifi_mgmt.h>

LOG_MODULE_REGISTER(wifi_mgr, LOG_LEVEL_DBG);

/* ── Kconfig values ───────────────────────────────────────────────────── */

#define CONNECT_TIMEOUT K_SECONDS(30)

/* ── Internal state ────────────────────────────────────────────────── */

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

/* Forward declarations */
static int wifi_mgr_init(void);
static int wifi_mgr_disconnect(void);
static bool wifi_mgr_is_connected(void);
static bool wifi_mgr_has_credentials(void);
static int wifi_mgr_connect_stored(void);

static const network_iface_t net_iface = {
	.init = wifi_mgr_init,
	.disconnect = wifi_mgr_disconnect,
	.is_connected = wifi_mgr_is_connected,
	.has_credentials = wifi_mgr_has_credentials,
	.connect = wifi_mgr_connect_stored,
};
static struct k_msgq *evt_queue;
static struct net_mgmt_event_callback mgmt_cb;
static struct net_if *iface;
static K_SEM_DEFINE(connect_sem, 0, 1);
static bool connected;

/* ── Net-Management Callbacks ────────────────────────────────────────── */

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
				   struct net_if *iface)
{
	if ((mgmt_event & EVENT_MASK) != mgmt_event) {
		return;
	}
	if (mgmt_event == NET_EVENT_L4_CONNECTED) {
		LOG_DBG("Received Wi-Fi connected event");
		connected = true;
		k_sem_give(&connect_sem);
		app_event_t connect_evt = {
			.type = EVT_WIFI_CONNECTED,
		};
		k_msgq_put(evt_queue, &connect_evt, K_NO_WAIT);
		return;
	}
	if (mgmt_event == NET_EVENT_L4_DISCONNECTED) {
		if (connected == false) {
			LOG_DBG("Waiting for Wi-Fi to be connected");
		} else {
			LOG_DBG("Wi-Fi disconnected");
			connected = false;
			app_event_t disconnect_evt = {
				.type = EVT_WIFI_DISCONNECTED,
			};
			k_msgq_put(evt_queue, &disconnect_evt, K_NO_WAIT);
		}
		k_sem_reset(&connect_sem);
		return;
	}
}

/**
 * Initializes the WiFi manager and registers net-management callbacks.
 * Must be called before wifi_connect().
 * @param event_queue Pointer to the app event message queue for posting WiFi
 * events.
 */
static int wifi_mgr_init(void)
{
	connected = false;

	LOG_DBG("Initializing Wi-Fi driver...");
	/* Sleep 1 sec. to allow initialization of Wi-Fi driver */
	k_sleep(K_SECONDS(1));
	iface = net_if_get_first_wifi();
	if (iface == NULL) {
		LOG_ERR("Returned network interface is NULL");
	}

	net_if_up(iface);
	/*     while (!net_if_is_up(iface))
	    {
		LOG_DBG("Waiting for Wi-Fi interface to be up...");
		k_sleep(K_SECONDS(1));
	    } */

	net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
	net_mgmt_add_event_callback(&mgmt_cb);

	return 0;
}

/**
 * Disconnects the Wi-Fi connection.
 * @return 0 on success,
 *   -ENOEXEC if disconnection failed,
 *   or -ENODEV if no Wi-Fi interface is available
 */
static int wifi_mgr_disconnect(void)
{
	struct net_if *iface = net_if_get_first_wifi();
	if (!iface) {
		return -ENODEV;
	}

	int err = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
	if (err) {
		LOG_ERR("Disconnecting from Wi-Fi failed, err: %d", err);
		return ENOEXEC;
	}

	connected = false;
	app_event_t disc_evt = {.type = EVT_WIFI_DISCONNECTED};
	k_msgq_put(evt_queue, &disc_evt, K_NO_WAIT);

	return err;
}

/**
 * Returns true if currently connected and an IP address is available.
 */
static bool wifi_mgr_is_connected(void)
{
	return connected;
}

/**
 * Checks if stored WiFi credentials exist.
 * @return true if at least one set of credentials is stored
 */
static bool wifi_mgr_has_credentials(void)
{
	return !wifi_credentials_is_empty();
}

/**
 * Connects to WiFi using stored credentials (provisioned via BLE).
 * Non-blocking — result delivered via EVT_WIFI_CONNECTED /
 * EVT_WIFI_CONNECT_FAILED.
 * @return 0 on success (connection initiated), negative error code on failure
 */
static int wifi_mgr_connect_stored(void)
{
	LOG_INF("Connecting with stored credentials...");
	struct net_if *wifi_iface = net_if_get_first_wifi();
	if (!wifi_iface) {
		LOG_ERR("No Wi-Fi interface available");
		return -ENODEV;
	}
	int err = net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, wifi_iface, NULL, 0);
	if (err) {
		LOG_ERR("Failed to initiate stored-credential connect, err: %d", err);
		return err;
	}
	return 0;
}

const network_iface_t *wifi_get_iface(struct k_msgq *msgq)
{
	evt_queue = msgq;
	return &net_iface;
}
