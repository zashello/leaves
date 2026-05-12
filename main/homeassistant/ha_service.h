#pragma once

#include "esp_err.h"

typedef void (*ai_trigger_callback_t)(void);

esp_err_t ha_service_start(void);
esp_err_t ha_service_stop(void);
void ha_service_set_ai_callback(ai_trigger_callback_t callback);
void ha_service_handle_mqtt_data(const char *topic, const char *data, int data_len);
