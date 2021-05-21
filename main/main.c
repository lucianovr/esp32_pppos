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
#define MAX_TOPIC_LEN               (64)
#define DEVICE_NAME()               "lucianovr"
#define DEVICE_FW_VERSION()         "1.3.4"
#define CONFIG_GET_MQTT_QOS()       (1)
#define CONFIG_GET_MQTT_RETAIN()    (0)


/* MQTT */
#define APP_MQTT_BROKER_URL 	"broker.mqttdashboard.com"
#define APP_MQTT_PORT           (1883)

// PRIVATE DATA TYPES
typedef enum{
    EVENT_TYPE_HEARTBEAT_TIMER,
    EVENT_TYPE_IF_UP,
    EVENT_TYPE_IF_DOWN,
    EVENT_TYPE_MQTT_UP,
    EVENT_TYPE_MQTT_DOWN,
    EVENT_TYPE_MQTT_PUBLISH,
    EVENT_TYPE_MQTT_RECEIVED,
} enum_event_type_t;

typedef struct{
    char topic[MAX_TOPIC_LEN];
    uint8_t data[256];
    size_t len;
} st_mqtt_pub_t;

typedef struct{
    char topic[MAX_TOPIC_LEN];
    uint8_t data[256];
    size_t len;
} st_mqtt_recv_t;

typedef struct{
    enum_event_type_t type;
    union{
        st_mqtt_pub_t mqtt_pub;
        st_mqtt_recv_t mqtt_recv;
    }u;
} st_event_t;

// PRIVATE VARIABLES
static QueueHandle_t event_queue;

// PRIVATE FUNCTIONS

static void app_start_tasks(void);
static void app_task(void* pv);
static void app_handle_event(st_event_t* event);
static void app_handle_init_esp(void);

static void app_handle_hearbeat_cb(void);
static void app_handle_if_connected(void);
static void app_handle_if_disconnected(void);
static void app_handle_mqtt_connected(void);
static void app_handle_mqtt_disconnected(void);

static void _app_heartbeat_timer_cb(TimerHandle_t xTimer);
static void _app_on_if_up(void);
static void _app_on_if_down(void);
static void _app_on_mqtt_conn(void);
static void _app_on_mqtt_disconn(void);

void app_main(void)
{
    app_handle_init_esp();

    comm_if_init();
    comm_if_reg_connected_cb( _app_on_if_up );
    comm_if_reg_disconnected_cb( _app_on_if_down );
    
    mqtt_init();
    mqtt_reg_connected_cb( _app_on_mqtt_conn );
    mqtt_reg_disconnected_cb( _app_on_mqtt_disconn );
    
    comm_if_start();

    app_start_tasks();
}

static void app_start_tasks(void){
    event_queue = xQueueCreate(10, sizeof(st_event_t));

    xTaskCreate(app_task, "main task", configMINIMAL_STACK_SIZE*4, NULL, configMAX_PRIORITIES-2, NULL);

    TimerHandle_t hb_timer = xTimerCreate("heartbeat", pdMS_TO_TICKS(10000), pdTRUE, NULL, _app_heartbeat_timer_cb);
    xTimerStart(hb_timer, 0);
}


// PRIVATE FUNCTIONS IMPLEMENTATIONS

static void app_task(void* pv){

    st_event_t event;
    while (1)
    {
        if (xQueueReceive(event_queue, &event, portMAX_DELAY) != pdTRUE)
            continue;

        app_handle_event(&event);
    }

    vTaskDelete(NULL);
}

static void app_handle_event(st_event_t* event){
    switch (event->type)
    {

    case EVENT_TYPE_HEARTBEAT_TIMER:
        app_handle_hearbeat_cb();
        break;
    case EVENT_TYPE_IF_UP:
        app_handle_if_connected();
        break;

    case EVENT_TYPE_IF_DOWN:
        app_handle_if_disconnected();
        break;

    case EVENT_TYPE_MQTT_UP:
        app_handle_mqtt_connected();
        break;

    case EVENT_TYPE_MQTT_DOWN:
        app_handle_mqtt_disconnected();
        break;

    case EVENT_TYPE_MQTT_PUBLISH:
        /* code */
        break;

    case EVENT_TYPE_MQTT_RECEIVED:
        /* code */
        break;

    default:
        break;
    }
}

static void app_handle_init_esp(void){
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

static void app_handle_hearbeat_cb(void){
    char topic[MAX_TOPIC_LEN];
    char buf[32];

    /* Only publish uptime when connected, we don't want it to be queued */
    if (!mqtt_is_connected())
        return;

    /* Uptime (in seconds) */
    sprintf(buf, "%lld", esp_timer_get_time() / 1000 / 1000);
    snprintf(topic, MAX_TOPIC_LEN, "%s/Uptime", DEVICE_NAME() );
    mqtt_publish(topic, CONFIG_GET_MQTT_QOS(), (uint8_t *)buf, strlen(buf));

    /* Free memory (in bytes) */
    sprintf(buf, "%u", esp_get_free_heap_size());
    snprintf(topic, MAX_TOPIC_LEN, "%s/FreeMemory", DEVICE_NAME());
    mqtt_publish(topic, CONFIG_GET_MQTT_QOS(), (uint8_t *)buf, strlen(buf));
}

static void app_handle_if_connected(void){
    ESP_LOGI( TAG, "interface: connected, connecting to MQTT" );
    mqtt_connect(APP_MQTT_BROKER_URL, APP_MQTT_PORT, NULL, NULL);
}

static void app_handle_if_disconnected(void){
    ESP_LOGW( TAG, "interface: disconnected, stopping MQTT" );
    mqtt_disconnect();
}

static void app_handle_mqtt_connected(void){
    ESP_LOGI( TAG, "mqtt: connected" );
}

static void app_handle_mqtt_disconnected(void){
    ESP_LOGW( TAG, "mqtt: disconnected" );
}

static void _app_heartbeat_timer_cb(TimerHandle_t xTimer){
    st_event_t event;

    event.type = EVENT_TYPE_HEARTBEAT_TIMER;
    xQueueSend(event_queue, &event, portMAX_DELAY);
}

static void _app_on_if_up(void){
    st_event_t event;

    event.type = EVENT_TYPE_IF_UP;
    xQueueSend(event_queue, &event, portMAX_DELAY);
}

static void _app_on_if_down(void){
    st_event_t event;

    event.type = EVENT_TYPE_IF_DOWN;
    xQueueSend(event_queue, &event, portMAX_DELAY);
}

static void _app_on_mqtt_conn(void){
    st_event_t event;

    event.type = EVENT_TYPE_MQTT_UP;
    xQueueSend(event_queue, &event, portMAX_DELAY);
}

static void _app_on_mqtt_disconn(void){
    st_event_t event;

    event.type = EVENT_TYPE_MQTT_DOWN;
    xQueueSend(event_queue, &event, portMAX_DELAY);
}

