#pragma once

#include <esp_err.h>
#include <stdbool.h>

typedef struct {
    uint16_t co2_ppm;
    float temperature_c;
    float humidity_rh;
    bool data_valid;
} scd41_data_t;

esp_err_t scd41_init(void);
esp_err_t scd41_read_data(scd41_data_t *data);
esp_err_t scd41_start_measurement(void);
esp_err_t scd41_stop_measurement(void);
void scd41_deinit(void);
