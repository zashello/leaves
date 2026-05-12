#pragma once

#include "esp_err.h"
#include <as7341.h>

esp_err_t ha_sensor_start(void);
esp_err_t ha_sensor_stop(void);
esp_err_t ha_sensor_publish_discovery(void);
esp_err_t ha_sensor_publish_data(as7341_channels_spectral_data_t *data);
