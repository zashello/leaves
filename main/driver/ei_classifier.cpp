#include <string.h>
#include "esp_log.h"
#include "ei_classifier.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

static const char *TAG = "EI_CLASSIFY";

static float g_features[EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME];
static bool g_initialized = false;

static void calculateVegetationIndices(
    const as7341_channels_spectral_data_t *spectralData, 
    float *ndvi, float *gndvi, float *psri)
{
    if (spectralData == NULL) {
        *ndvi = 0.0f;
        *gndvi = 0.0f;
        *psri = 0.0f;
        return;
    }

    if ((spectralData->f8 + spectralData->f5) != 0) {
        *ndvi = (float)(spectralData->f8 - spectralData->f5) / 
                (float)(spectralData->f8 + spectralData->f5);
    } else {
        *ndvi = 0.0f;
    }

    if ((spectralData->nir + spectralData->f7) != 0) {
        *gndvi = (float)(spectralData->nir - spectralData->f7) / 
                 (float)(spectralData->nir + spectralData->f7);
    } else {
        *gndvi = 0.0f;
    }

    if (spectralData->f8 != 0) {
        *psri = (float)(spectralData->f7 - spectralData->f5) / 
                (float)spectralData->f8;
    } else {
        *psri = 0.0f;
    }
}

static int featureCallback(size_t offset, size_t length, float *out_ptr)
{
    if (offset + length > EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) {
        return -2;
    }
    memcpy(out_ptr, g_features + offset, length * sizeof(float));
    return 0;
}

esp_err_t eiClassifierInit(void)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "EI分类器已初始化");
        return ESP_OK;
    }

    run_classifier_init();
    g_initialized = true;

    ESP_LOGI(TAG, "EI分类器初始化完成 (标签: Diseased/Healthy/Low_Nutrient/Low_Water)");
    return ESP_OK;
}

esp_err_t eiClassifierRun(const as7341_channels_spectral_data_t *spectralData, ei_inference_result_t *result)
{
    if (spectralData == NULL || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_initialized) {
        ESP_LOGE(TAG, "EI分类器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    g_features[0] = (float)spectralData->f1;
    g_features[1] = (float)spectralData->f2;
    g_features[2] = (float)spectralData->f3;
    g_features[3] = (float)spectralData->f4;
    g_features[4] = (float)spectralData->f5;
    g_features[5] = (float)spectralData->f6;
    g_features[6] = (float)spectralData->f7;
    g_features[7] = (float)spectralData->f8;
    g_features[8] = (float)spectralData->nir;
    g_features[9] = (float)spectralData->clear;

    float ndvi, gndvi, psri;
    calculateVegetationIndices(spectralData, &ndvi, &gndvi, &psri);
    g_features[10] = ndvi;
    g_features[11] = gndvi;
    g_features[12] = psri;

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
    signal.get_data = featureCallback;

    ei_impulse_result_t eiResult;
    EI_IMPULSE_ERROR err = run_classifier(&signal, &eiResult, false);
    if (err != EI_IMPULSE_OK) {
        ESP_LOGE(TAG, "推理失败: %d", (int)err);
        return ESP_FAIL;
    }

    memset(result, 0, sizeof(ei_inference_result_t));

    float bestVal = -1.0f;
    int bestIdx = 0;
    for (size_t i = 0; i < EI_CLASS_COUNT && i < ei_default_impulse.impulse->label_count; i++) {
        result->results[i].label = eiResult.classification[i].label;
        result->results[i].value = eiResult.classification[i].value;
        if (eiResult.classification[i].value > bestVal) {
            bestVal = eiResult.classification[i].value;
            bestIdx = (int)i;
        }
    }

    result->bestLabel = result->results[bestIdx].label;
    result->bestValue = result->results[bestIdx].value;
    result->dspMs = eiResult.timing.dsp;
    result->classifyMs = eiResult.timing.classification;

    ESP_LOGI(TAG, "推理完成: %s (%.1f%%) DSP=%dms 推理=%dms",
             result->bestLabel, result->bestValue * 100.0f,
             result->dspMs, result->classifyMs);

    for (int i = 0; i < EI_CLASS_COUNT; i++) {
        ESP_LOGI(TAG, "  %s: %.1f%%", result->results[i].label, result->results[i].value * 100.0f);
    }

    return ESP_OK;
}
