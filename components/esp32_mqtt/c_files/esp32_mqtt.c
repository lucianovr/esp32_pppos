#include "esp32_mqtt.h"
#include "mqtt_client.h"
#include "esp_log.h"


// PRIVATE CONSTANTS
#define MQTT_RETAIN			(0)

#define TAG "esp32_mqtt"

// PRIVATE DATA TYPES

// PRIVATE VARIABLES
static esp_mqtt_client_handle_t mqtt_client_hdl = NULL;
static mqtt_connected_cb my_mqtt_connected = NULL;
static mqtt_disconnected_cb my_mqtt_disconnected = NULL;
static bool is_connected = false;

// PRIVATE FUNCTIONS
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);


// PUBLIC FUNCTIONS IMPLEMENTATIONS

bool mqtt_init(void)
{
    mqtt_client_hdl = NULL;
    return true;
}

bool mqtt_connect(char* url, uint16_t port, char* user, char* password)
{
    if( mqtt_client_hdl != NULL )
    {
        mqtt_disconnect();
    }
    /* Config MQTT */
    esp_mqtt_client_config_t mqtt_config = {
        .host = url,
        .port = port,
        .event_handle = mqtt_event_handler,
    };
    mqtt_client_hdl = esp_mqtt_client_init(&mqtt_config);

    ESP_ERROR_CHECK( esp_mqtt_client_start(mqtt_client_hdl ) );
    
    return true;
}

bool mqtt_disconnect( void )
{
    if( is_connected ){
        esp_mqtt_client_disconnect(mqtt_client_hdl);
        mqtt_client_hdl = NULL;
    }
    return true;
}

bool mqtt_is_connected( void )
{
    return is_connected;
}

bool mqtt_publish(char* topic, uint8_t qos, uint8_t* data, size_t len)
{
    int msg_id = esp_mqtt_client_publish(mqtt_client_hdl, topic, (const char*)data, len, qos, MQTT_RETAIN);
    return (msg_id != -1);
}

bool mqtt_subscribe(char* topic, uint8_t qos, mqtt_rxdatacb_t cb)
{
    int msg_id;
    msg_id = esp_mqtt_client_subscribe(mqtt_client_hdl, topic, qos);
    ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

    return (msg_id != -1);
}

void mqtt_reg_connected_cb( mqtt_connected_cb cb )
{
    my_mqtt_connected = cb;
}

void mqtt_reg_disconnected_cb( mqtt_disconnected_cb cb )
{
    my_mqtt_disconnected = cb;
}


// PRIVATE FUNCTION IMPLEMENTATION
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        is_connected = true;
        if( my_mqtt_connected != NULL )
            my_mqtt_connected();
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        is_connected = false;
        if( my_mqtt_disconnected != NULL )
            my_mqtt_disconnected();
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        ESP_LOG_BUFFER_HEXDUMP(TAG, event->data, event->data_len, ESP_LOG_INFO);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR, reason = %d", event->error_handle->error_type);
        if(event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED){
            ESP_LOGE(TAG, "MQTT Connect error = %d", event->error_handle->connect_return_code);
        }
        break;
    default:
        ESP_LOGI(TAG, "MQTT other event id: %d", event->event_id);
        break;
    }
    return ESP_OK;
}
