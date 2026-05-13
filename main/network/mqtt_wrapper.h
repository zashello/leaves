#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_RECONNECTING
} mqtt_state_t;

esp_err_t mqttClientInit(void);
esp_err_t mqttClientDeinit(void);
esp_err_t mqttClientConnect(void);
esp_err_t mqttClientDisconnect(void);
bool mqttClientIsConnected(void);
mqtt_state_t mqttClientGetState(void);
esp_err_t mqttClientPublish(const char *topic, const char *data, int qos, int retain);
esp_err_t mqttClientSubscribe(const char *topic, int qos);

typedef void (*mqtt_data_callback_t)(const char *topic, const char *data, int dataLen);
void mqttClientSetDataCallback(mqtt_data_callback_t callback);
