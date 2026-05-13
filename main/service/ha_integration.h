#pragma once

#include "esp_err.h"

esp_err_t haIntegrationStart(void);
void haIntegrationHandleMqtt(const char *topic, const char *data, int dataLen);
void haIntegrationSetAiCallback(void (*callback)(void));
void haIntegrationSetSensorCallback(void (*callback)(void));
