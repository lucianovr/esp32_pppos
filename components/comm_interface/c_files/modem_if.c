#include "comm_if.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_modem.h"
#include "esp_modem_netif.h"
#include "esp_log.h"
#include "sim800.h"
#include "bg96.h"
#include "sim7600.h"
#include "exs82w.h"

// PRIVATE CONSTANTS
#define TAG "comm_if"

// PRIVATE DATA TYPES

// PRIVATE VARIABLES
static modem_dce_t *dce = NULL;
static modem_dte_t *dte = NULL;
static esp_netif_t *esp_netif = NULL;
static void *modem_netif_adapter = NULL;

static comm_if_connected_cb_t connected_cb = NULL;
static comm_if_disconnected_cb_t disconnected_cb = NULL;

// PRIVATE FUNCTIONS
static void modem_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void on_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

// PUBLIC FUNCTIONS IMPLEMENTATIONS

void comm_if_init(void)
{
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    /* create dte object */
    esp_modem_dte_config_t config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    /* setup UART specific configuration based on kconfig options */
    config.tx_io_num = CONFIG_EXAMPLE_MODEM_UART_TX_PIN;
    config.rx_io_num = CONFIG_EXAMPLE_MODEM_UART_RX_PIN;
    config.rts_io_num = CONFIG_EXAMPLE_MODEM_UART_RTS_PIN;
    config.cts_io_num = CONFIG_EXAMPLE_MODEM_UART_CTS_PIN;
    config.rx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE;
    config.tx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_TX_BUFFER_SIZE;
    config.pattern_queue_size = CONFIG_EXAMPLE_MODEM_UART_PATTERN_QUEUE_SIZE;
    config.event_queue_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_QUEUE_SIZE;
    config.event_task_stack_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_STACK_SIZE;
    config.event_task_priority = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_PRIORITY;
    config.line_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE / 2;

    dte = esp_modem_dte_init(&config);
    /* Register event handler */
    ESP_ERROR_CHECK(esp_modem_set_event_handler(dte, modem_event_handler, ESP_EVENT_ANY_ID, NULL));

    // Init netif object
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
    esp_netif = esp_netif_new(&cfg);
    assert(esp_netif);

    modem_netif_adapter = esp_modem_netif_setup(dte);
    esp_modem_netif_set_default_handlers(modem_netif_adapter, esp_netif);
}

void comm_if_deinit(void)
{
    /* Unregister events, destroy the netif adapter and destroy its esp-netif instance */
    esp_modem_netif_clear_default_handlers(modem_netif_adapter);
    esp_modem_netif_teardown(modem_netif_adapter);
    esp_netif_destroy(esp_netif);
    ESP_ERROR_CHECK(dce->deinit(dce));
}

void comm_if_start(void)
{
#if CONFIG_LWIP_PPP_PAP_SUPPORT
    esp_netif_auth_type_t auth_type = NETIF_PPP_AUTHTYPE_PAP;
#elif CONFIG_LWIP_PPP_CHAP_SUPPORT
    esp_netif_auth_type_t auth_type = NETIF_PPP_AUTHTYPE_CHAP;
#elif !defined(CONFIG_EXAMPLE_MODEM_PPP_AUTH_NONE)
    #error "Unsupported AUTH Negotiation"
#endif


    /* create dce object */
#if CONFIG_EXAMPLE_MODEM_DEVICE_SIM800
    dce = sim800_init(dte);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_BG96
    dce = bg96_init(dte);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM7600
    dce = sim7600_init(dte);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_EXS82W
    dce = exs82w_init(dte);
#else
#error "Unsupported DCE"
#endif
    assert(dce != NULL);
//        ESP_ERROR_CHECK(dce->set_flow_ctrl(dce, MODEM_FLOW_CONTROL_NONE));
//        ESP_ERROR_CHECK(dce->store_profile(dce));

    /* Print Module ID, Operator, RAT, IMEI, IMSI */
    ESP_LOGI(TAG, "Module: %s", dce->name);
    ESP_LOGI(TAG, "Operator: %s, RAT: %d", dce->oper, dce->act);
    ESP_LOGI(TAG, "IMEI: %s", dce->imei);
    ESP_LOGI(TAG, "IMSI: %s", dce->imsi);

    /* Get signal quality */
    uint32_t rssi = 0, ber = 0;
    ESP_ERROR_CHECK(dce->get_signal_quality(dce, &rssi, &ber));
    ESP_LOGI(TAG, "rssi: %d, ber: %d", rssi, ber);


    /* setup PPPoS network parameters */
#if !defined(CONFIG_EXAMPLE_MODEM_PPP_AUTH_NONE) && (defined(CONFIG_LWIP_PPP_PAP_SUPPORT) || defined(CONFIG_LWIP_PPP_CHAP_SUPPORT))
    esp_netif_ppp_set_auth(esp_netif, auth_type, CONFIG_EXAMPLE_MODEM_PPP_AUTH_USERNAME, CONFIG_EXAMPLE_MODEM_PPP_AUTH_PASSWORD);
#endif
    /* attach the modem to the network interface */
    esp_netif_attach(esp_netif, modem_netif_adapter);
}

void comm_if_stop(void)
{
    /* Exit PPP mode */
    ESP_ERROR_CHECK(esp_modem_stop_ppp(dte));
}

void comm_if_power_down(void)
{
    /* Power down module */
    ESP_ERROR_CHECK(dce->power_down(dce));
    ESP_LOGI(TAG, "Power down");
}

void comm_if_reg_connected_cb(comm_if_connected_cb_t cb)
{
    connected_cb = cb;
}

void comm_if_reg_disconnected_cb(comm_if_disconnected_cb_t cb)
{
    disconnected_cb = cb;
}


// PRIVATE FUNCTION IMPLEMENTATION
static void modem_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ESP_MODEM_EVENT_PPP_START:
        ESP_LOGI(TAG, "Modem PPP Started");
        break;
    case ESP_MODEM_EVENT_PPP_STOP:
        ESP_LOGI(TAG, "Modem PPP Stopped");
        break;
    case ESP_MODEM_EVENT_UNKNOWN:
        ESP_LOGW(TAG, "Unknow line received: %s", (char *)event_data);
        break;
    default:
        break;
    }
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %d", event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        /* User interrupted event from esp-netif */
        esp_netif_t *netif = *(esp_netif_t**)event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %d", event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");

        ESP_LOGI(TAG, "GOT ip event!!!");
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "GOT IPv6 event!");

        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}