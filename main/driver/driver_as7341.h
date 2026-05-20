#pragma once

#include <esp_err.h>
#include <as7341.h>
#include <stdbool.h>

esp_err_t as7341Init(void);
esp_err_t as7341ReadData(as7341_channels_spectral_data_t *data);
char* as7341DataToJson(const as7341_channels_spectral_data_t *data);
void as7341Deinit(void);
esp_err_t as7341LedControl(bool enable);
esp_err_t as7341SetLedCurrent(as7341_led_drive_strengths_t current);
esp_err_t as7341SetLedCurrentMA(int current_mA);
esp_err_t as7341GetLedStatus(bool *enabled, as7341_led_drive_strengths_t *current);
esp_err_t as7341VerifyHardware(void);
void as7341TestLedCurrent(void);
bool as7341IsInitialized(void);
