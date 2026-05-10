#pragma once

#include <esp_err.h>
#include <as7341.h>

esp_err_t sensor_init(void);
esp_err_t sensor_read_data(as7341_channels_spectral_data_t *data);
char* sensor_data_to_json(const as7341_channels_spectral_data_t *data);
void sensor_deinit(void);
