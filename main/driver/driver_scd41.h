#pragma once

#include <esp_err.h>
#include <stdbool.h>

typedef struct {
    uint16_t co2_ppm;
    float temperature_c;
    float humidity_rh;
    bool data_valid;
} scd41_data_t;

esp_err_t scd41Init(void);
esp_err_t scd41ReadData(scd41_data_t *data);
void scd41Deinit(void);
