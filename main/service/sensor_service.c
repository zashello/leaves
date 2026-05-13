#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "app_config.h"
#include "storage/storage.h"
#include "network/mqtt_wrapper.h"
#include "driver/driver_as7341.h"
#include "driver/driver_scd41.h"
#include "sensor_service.h"

static const char *TAG = "SENSOR_SVC";

static TaskHandle_t g_task = NULL;
static bool g_running = false;

static void publishDiscovery(const char *sensorId, const char *name, const char *deviceClass, const char *unit)
{
    device_config_t config;
    if (storageLoad(&config) != ESP_OK) return;

    char topic[128];
    snprintf(topic, sizeof(topic), "%s/sensor/%s_%s/config",
             MQTT_DISCOVERY_PREFIX, config.deviceName, sensorId);

    char stateTopic[64];
    snprintf(stateTopic, sizeof(stateTopic), "%s/sensor/%s", MQTT_SENSOR_BASE, sensorId);

    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "name", MQTT_DEVICE_NAME);
    cJSON_AddStringToObject(device, "identifiers", config.deviceName);
    cJSON_AddStringToObject(device, "manufacturer", MQTT_MANUFACTURER);
    cJSON_AddStringToObject(device, "model", MQTT_MODEL);

    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "name", name);
    cJSON_AddStringToObject(payload, "state_topic", stateTopic);
    char uniqueId[64];
    snprintf(uniqueId, sizeof(uniqueId), "%s_%s", config.deviceName, sensorId);
    cJSON_AddStringToObject(payload, "unique_id", uniqueId);
    cJSON_AddItemToObject(payload, "device", device);

    if (deviceClass) cJSON_AddStringToObject(payload, "device_class", deviceClass);
    if (unit) cJSON_AddStringToObject(payload, "unit_of_measurement", unit);

    char *payloadStr = cJSON_PrintUnformatted(payload);
    mqttClientPublish(topic, payloadStr, 1, 1);

    cJSON_Delete(payload);
    free(payloadStr);
}

static void publishFloatValue(const char *sensorId, float value, const char *fmt)
{
    char topic[64];
    char valueStr[32];
    snprintf(topic, sizeof(topic), "%s/sensor/%s", MQTT_SENSOR_BASE, sensorId);
    snprintf(valueStr, sizeof(valueStr), fmt, value);
    mqttClientPublish(topic, valueStr, 1, 1);
}

static void publishUintValue(const char *sensorId, unsigned int value)
{
    char topic[64];
    char valueStr[32];
    snprintf(topic, sizeof(topic), "%s/sensor/%s", MQTT_SENSOR_BASE, sensorId);
    snprintf(valueStr, sizeof(valueStr), "%u", value);
    mqttClientPublish(topic, valueStr, 1, 1);
}

static void sensorTask(void *param)
{
    const TickType_t interval = pdMS_TO_TICKS(MQTT_SENSOR_INTERVAL);
    const TickType_t initialDelay = pdMS_TO_TICKS(5000);

    ESP_LOGI(TAG, "等待5秒后开始传感器任务...");
    vTaskDelay(initialDelay);

    if (!g_running) {
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "发布传感器发现配置...");
    sensorServicePublishDiscovery();

    while (g_running) {
        vTaskDelay(interval);
        if (!g_running) break;
        if (!mqttClientIsConnected()) {
            ESP_LOGW(TAG, "MQTT未连接，跳过本次上报");
            continue;
        }

        esp_err_t ret = as7341Init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "AS7341初始化失败");
            continue;
        }

        as7341_channels_spectral_data_t spectralData;
        ret = as7341ReadData(&spectralData);
        if (ret == ESP_OK) {
            sensorServicePublishSpectralData(&spectralData);
        } else {
            ESP_LOGE(TAG, "AS7341读取失败");
        }
        as7341Deinit();

        scd41_data_t envData;
        ret = scd41ReadData(&envData);
        if (ret == ESP_OK && envData.data_valid) {
            sensorServicePublishEnvironmentData(&envData);
        } else {
            ESP_LOGW(TAG, "SCD41数据读取失败");
        }

        ESP_LOGI(TAG, "传感器数据已上报");
    }

    g_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t sensorServiceStart(void)
{
    if (g_running) {
        ESP_LOGW(TAG, "传感器任务已在运行");
        return ESP_OK;
    }

    g_running = true;
    BaseType_t ret = xTaskCreate(sensorTask, "sensor_svc", 8192, NULL, 4, &g_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "传感器任务创建失败");
        g_running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "传感器任务已启动");
    return ESP_OK;
}

esp_err_t sensorServiceStop(void)
{
    if (!g_running) return ESP_OK;
    g_running = false;
    if (g_task != NULL) vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "传感器任务已停止");
    return ESP_OK;
}

esp_err_t sensorServicePublishDiscovery(void)
{
    publishDiscovery("f1", "Plant F1 (415-445nm)", NULL, NULL);
    publishDiscovery("f2", "Plant F2 (445-480nm)", NULL, NULL);
    publishDiscovery("f3", "Plant F3 (480-520nm)", NULL, NULL);
    publishDiscovery("f4", "Plant F4 (520-550nm)", NULL, NULL);
    publishDiscovery("f5", "Plant F5 (550-585nm)", NULL, NULL);
    publishDiscovery("f6", "Plant F6 (585-630nm)", NULL, NULL);
    publishDiscovery("f7", "Plant F7 (630-680nm)", NULL, NULL);
    publishDiscovery("f8", "Plant F8 (680-720nm)", NULL, NULL);
    publishDiscovery("clear", "Plant Clear Channel", NULL, NULL);
    publishDiscovery("nir", "Plant NIR", NULL, NULL);
    publishDiscovery("ndvi", "Plant NDVI", NULL, NULL);
    publishDiscovery("sipi", "Plant SIPI", NULL, NULL);
    publishDiscovery("psri", "Plant PSRI", NULL, NULL);
    publishDiscovery("cri550", "Plant CRI550", NULL, NULL);
    publishDiscovery("cri700", "Plant CRI700", NULL, NULL);
    publishDiscovery("co2", "CO2 Concentration", "carbon_dioxide", "ppm");
    publishDiscovery("temperature", "Temperature", "temperature", "\xC2\xB0""C");
    publishDiscovery("humidity", "Humidity", "humidity", "%");

    ESP_LOGI(TAG, "所有传感器发现配置已发布");
    return ESP_OK;
}

esp_err_t sensorServicePublishSpectralData(as7341_channels_spectral_data_t *data)
{
    if (data == NULL) return ESP_ERR_INVALID_ARG;

    publishUintValue("f1", data->f1);
    publishUintValue("f2", data->f2);
    publishUintValue("f3", data->f3);
    publishUintValue("f4", data->f4);
    publishUintValue("f5", data->f5);
    publishUintValue("f6", data->f6);
    publishUintValue("f7", data->f7);
    publishUintValue("f8", data->f8);
    publishUintValue("clear", data->clear);
    publishUintValue("nir", data->nir);

    float ndvi = 0.0f;
    if ((data->f8 + data->f5) != 0) ndvi = (float)(data->f8 - data->f5) / (float)(data->f8 + data->f5);
    publishFloatValue("ndvi", ndvi, "%.4f");

    float sipi = 0.0f;
    if ((data->nir - data->f7) != 0) sipi = (float)(data->nir - data->f2) / (float)(data->nir - data->f7);
    publishFloatValue("sipi", sipi, "%.4f");

    float psri = 0.0f;
    if (data->f8 != 0) psri = (float)(data->f7 - data->f5) / (float)data->f8;
    publishFloatValue("psri", psri, "%.4f");

    float cri550 = 0.0f;
    if (data->f3 != 0 && data->f5 != 0) cri550 = (1.0f / (float)data->f3) - (1.0f / (float)data->f5);
    publishFloatValue("cri550", cri550, "%.6f");

    float cri700 = 0.0f;
    if (data->f3 != 0 && data->f8 != 0) cri700 = (1.0f / (float)data->f3) - (1.0f / (float)data->f8);
    publishFloatValue("cri700", cri700, "%.6f");

    return ESP_OK;
}

esp_err_t sensorServicePublishEnvironmentData(const scd41_data_t *data)
{
    if (data == NULL || !data->data_valid) return ESP_ERR_INVALID_ARG;

    publishUintValue("co2", data->co2_ppm);
    publishFloatValue("temperature", data->temperature_c, "%.2f");
    publishFloatValue("humidity", data->humidity_rh, "%.2f");

    return ESP_OK;
}
