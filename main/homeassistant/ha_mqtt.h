#pragma once

#include "esp_err.h"
#include "mqtt_client.h"
#include "ha_config.h"

esp_err_t ha_mqtt_init(void);
esp_err_t ha_mqtt_deinit(void);
esp_err_t ha_mqtt_connect(void);
esp_err_t ha_mqtt_disconnect(void);
bool ha_mqtt_is_connected(void);
ha_mqtt_state_t ha_mqtt_get_state(void);
esp_err_t ha_mqtt_publish(const char *topic, const char *data, int qos, int retain);
esp_err_t ha_mqtt_subscribe(const char *topic, int qos);
esp_err_t ha_mqtt_set_callback(esp_mqtt_client_handle_t client);
