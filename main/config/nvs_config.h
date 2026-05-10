#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define WIFI_SSID_MAX_LEN    32
#define WIFI_PASS_MAX_LEN    64
#define API_KEY_MAX_LEN      128
#define SENDKEY_MAX_LEN      64

#define PROVISION_SSID       "Leaves_Setup"
#define PROVISION_PASS       "12345678"

typedef struct {
    char wifiSsid[WIFI_SSID_MAX_LEN];
    char wifiPass[WIFI_PASS_MAX_LEN];
    char siliconflowKey[API_KEY_MAX_LEN];
    char serverchanKey[SENDKEY_MAX_LEN];
    bool configValid;
} device_config_t;

esp_err_t configInit(void);
esp_err_t configLoad(device_config_t *config);
esp_err_t configSave(const device_config_t *config);
esp_err_t configClear(void);
bool configIsValid(void);
