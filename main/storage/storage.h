#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "app_config.h"

esp_err_t storageInit(void);
esp_err_t storageLoad(device_config_t *config);
esp_err_t storageSave(const device_config_t *config);
esp_err_t storageClear(void);
bool storageIsValid(void);
