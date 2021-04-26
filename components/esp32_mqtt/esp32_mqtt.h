#ifndef ESP32_MQTT_H_
#define ESP32_MQTT_H_

#include <stdint.h>
#include <stdbool.h>
#include <strings.h>

typedef void(*mqtt_rxdatacb_t)(uint8_t* data, size_t len);
typedef void(*mqtt_connected_cb)(void);
typedef void(*mqtt_disconnected_cb)(void);

bool mqtt_init(void);
bool mqtt_connect(char* url, uint16_t port, char* user, char* password);
bool mqtt_disconnect( void );
bool mqtt_publish(char* topic, uint8_t qos, uint8_t* data, size_t len);
bool mqtt_subscribe(char* topic, uint8_t qos, mqtt_rxdatacb_t cb);

void mqtt_reg_connected_cb( mqtt_connected_cb cb );
void mqtt_reg_disconnected_cb( mqtt_disconnected_cb cb );



#endif // ESP32_MQTT_H_