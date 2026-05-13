#pragma once

#include <esp_err.h>
#include <as7341.h>

#define EI_CLASS_COUNT 3

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *label;
    float value;
} ei_classification_result_t;

typedef struct {
    ei_classification_result_t results[EI_CLASS_COUNT];
    const char *bestLabel;
    float bestValue;
    int dspMs;
    int classifyMs;
} ei_inference_result_t;

esp_err_t eiClassifierInit(void);
esp_err_t eiClassifierRun(const as7341_channels_spectral_data_t *spectralData, ei_inference_result_t *result);

#ifdef __cplusplus
}
#endif
