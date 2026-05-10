#pragma once

#include <stdbool.h>
#include "config/nvs_config.h"

esp_err_t wifiConnectWithConfig(const device_config_t *config);
bool wifiWaitConnected(uint32_t timeoutMs);
