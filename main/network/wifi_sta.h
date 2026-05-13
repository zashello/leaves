#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "app_config.h"

esp_err_t wifiStaConnect(const device_config_t *config);
bool wifiStaWaitConnected(uint32_t timeoutMs);
bool wifiStaIsConnected(void);
