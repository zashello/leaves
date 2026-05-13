#pragma once

#include <esp_err.h>
#include <as7341.h>

esp_err_t as7341Init(void);
esp_err_t as7341ReadData(as7341_channels_spectral_data_t *data);
char* as7341DataToJson(const as7341_channels_spectral_data_t *data);
void as7341Deinit(void);
