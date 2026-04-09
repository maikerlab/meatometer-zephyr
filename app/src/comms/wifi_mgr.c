#include "wifi_mgr.h"
#include "app_events.h"
#include <zephyr/kernel.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wifi_mgr, LOG_LEVEL_DBG);

/* ── Kconfig values ───────────────────────────────────────────────────── */

#define CONNECT_TIMEOUT K_SECONDS(30)

/* ── Internal state ────────────────────────────────────────────────── */

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
static struct k_msgq *evt_queue;
static struct net_mgmt_event_callback mgmt_cb;
static struct net_if *iface;
static K_SEM_DEFINE(connect_sem, 0, 1);
static bool connected;

/* ── Net-Management Callbacks ────────────────────────────────────────── */

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event, struct net_if *iface)
{
    if ((mgmt_event & EVENT_MASK) != mgmt_event)
    {
        return;
    }
    if (mgmt_event == NET_EVENT_L4_CONNECTED)
    {
        LOG_DBG("Received Wi-Fi connected event");
        connected = true;
        k_sem_give(&connect_sem);
        return;
    }
    if (mgmt_event == NET_EVENT_L4_DISCONNECTED)
    {
        if (connected == false)
        {
            LOG_DBG("Waiting for Wi-Fi to be connected");
        }
        else
        {
            LOG_DBG("Wi-Fi disconnected");
            connected = false;
        }
        k_sem_reset(&connect_sem);
        return;
    }
}

static int wifi_args_to_params(struct wifi_connect_req_params *params)
{
    /* Populate SSID and password from Zephyr's WiFi credentials configuration */
    params->ssid = CONFIG_APP_WIFI_SSID;
    params->ssid_length = strlen(params->ssid);

    params->psk = CONFIG_APP_WIFI_PASSWORD;
    params->psk_length = strlen(params->psk);

    // Populate the rest of the relevant members
    params->channel = WIFI_CHANNEL_ANY;
    params->security = WIFI_SECURITY_TYPE_PSK;
    params->mfp = WIFI_MFP_OPTIONAL;
    params->timeout = SYS_FOREVER_MS;
    params->band = WIFI_FREQ_BAND_UNKNOWN;
    memset(params->bssid, 0, sizeof(params->bssid));
    return 0;
}

/* ── Public API ─────────────────────────────────────────────────── */

void wifi_mgr_init(struct k_msgq *queue)
{
    evt_queue = queue;
    connected = false;

    LOG_DBG("Initializing Wi-Fi driver...");
    /* Sleep 1 sec. to allow initialization of Wi-Fi driver */
    k_sleep(K_SECONDS(1));
    iface = net_if_get_first_wifi();
    if (iface == NULL)
    {
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
}

int wifi_mgr_connect(void)
{
    LOG_INF("Connecting to SSID: %s", CONFIG_APP_WIFI_SSID);

    struct wifi_connect_req_params cnx_params;

    wifi_args_to_params(&cnx_params);

    net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(struct wifi_connect_req_params));

    // Wait for DHCP or timeout
    if (k_sem_take(&connect_sem, CONNECT_TIMEOUT) != 0)
    {
        LOG_ERR("WiFi connect timeout!");

        app_event_t fail_evt = {
            .type = EVT_WIFI_CONNECT_FAILED,
            .data.error_code = -ETIMEDOUT,
        };
        k_msgq_put(evt_queue, &fail_evt, K_NO_WAIT);
        return -ETIMEDOUT;
    }

    if (!connected)
    {
        LOG_ERR("WiFi connection failed!");
        return -ECONNREFUSED;
    }

    LOG_INF("Wi-Fi connected!");
    struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;
    if (!ipv4)
    {
        LOG_ERR("No IPv4 address assigned to Wi-Fi interface");
        return -EADDRNOTAVAIL;
    }
    char addr[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET,
                  &ipv4->unicast[0].ipv4.address.in_addr,
                  addr, sizeof(addr));
    LOG_INF("Own IP: %s", addr);

    app_event_t conn_evt = {.type = EVT_WIFI_CONNECTED};
    k_msgq_put(evt_queue, &conn_evt, K_NO_WAIT);

    return 0;
}

int wifi_mgr_disconnect(void)
{
    struct net_if *iface = net_if_get_first_wifi();
    if (!iface)
    {
        return -ENODEV;
    }

    int err = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
    if (err)
    {
        LOG_ERR("Disconnecting from Wi-Fi failed, err: %d", err);
        return ENOEXEC;
    }

    connected = false;
    app_event_t disc_evt = {.type = EVT_WIFI_DISCONNECTED};
    k_msgq_put(evt_queue, &disc_evt, K_NO_WAIT);

    return err;
}

bool wifi_mgr_is_connected(void)
{
    return connected;
}