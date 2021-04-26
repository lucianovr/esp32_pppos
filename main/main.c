#include <string.h>
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"


#include "comm_if.h"
#include "esp32_mqtt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// PRIVATE CONSTANTS
static const char *TAG = "main";

/* MQTT */
#define APP_MQTT_BROKER_URL 	"mqtt://broker.hivemq.com"
#define APP_MQTT_PORT           (1883)
#define APP_MQTT_QOS			(1)
#define APP_MQTT_TOPIC          "/lucianovr/pppos"
#define APP_MQTT_DATA           "luciano"

// PRIVATE DATA TYPES
// PRIVATE VARIABLES
// PRIVATE FUNCTIONS

static void app_if_connected(void);
static void app_if_disconnected(void);
static void app_mqtt_connected(void);
static void app_mqtt_disconnected(void);

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    comm_if_init();
    comm_if_reg_connected_cb( app_if_connected );
    comm_if_reg_disconnected_cb( app_if_disconnected );
    
    mqtt_init();
    mqtt_reg_connected_cb( app_mqtt_connected );
    mqtt_reg_disconnected_cb( app_mqtt_disconnected );
    
    comm_if_start();
}


// PRIVATE FUNCTIONS IMPLEMENTATIONS

static void app_if_connected(void){
    ESP_LOGI( TAG, "interface: connected" );
    mqtt_connect(APP_MQTT_BROKER_URL, APP_MQTT_PORT, NULL, NULL);
}

static void app_if_disconnected(void){
    ESP_LOGW( TAG, "interface: disconnected" );
    mqtt_disconnect();
}

static void app_mqtt_connected(void){
    ESP_LOGI( TAG, "mqtt: connected" );
    mqtt_subscribe(APP_MQTT_TOPIC, 1, NULL);
    mqtt_publish(APP_MQTT_TOPIC, APP_MQTT_QOS, (uint8_t*)APP_MQTT_DATA, strlen(APP_MQTT_DATA));
}

static void app_mqtt_disconnected(void){
    ESP_LOGW( TAG, "mqtt: disconnected" );
    mqtt_connect(APP_MQTT_BROKER_URL, APP_MQTT_PORT, NULL, NULL);
}