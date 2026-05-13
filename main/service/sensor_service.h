#pragma once

#include <esp_err.h>
#include <as7341.h>
#include "driver/driver_scd41.h"

esp_err_t sensorServiceStart(void);
esp_err_t sensorServiceStop(void);
esp_err_t sensorServicePublishDiscovery(void);
esp_err_t sensorServicePublishSpectralData(as7341_channels_spectral_data_t *data);
esp_err_t sensorServicePublishEnvironmentData(const scd41_data_t *data);
void sensorServiceReportOnce(void);
