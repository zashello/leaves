#include <string.h>
#include "ha_sensor.h"
#include "ha_mqtt.h"
#include "ha_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "config/nvs_config.h"
#include "hardware/scd41.h"

static const char *TAG = "HA_SENSOR";

static TaskHandle_t g_sensor_task = NULL;
static bool g_sensor_running = false;

static void publishSensorDiscovery(const char *sensor_id, const char *name, const char *device_class, const char *unit);
static void sensorTask(void *param);

static void publishSensorDiscovery(const char *sensor_id, const char *name, const char *device_class, const char *unit) {
    device_config_t config;
    if (configLoad(&config) != ESP_OK) {
        return;
    }

    char topic[128];
    snprintf(topic, sizeof(topic), "%s/sensor/%s_%s/config",
             MQTT_DISCOVERY_PREFIX, config.deviceName, sensor_id);

    char state_topic[64];
    snprintf(state_topic, sizeof(state_topic), "%s/sensor/%s", MQTT_SENSOR_BASE_TOPIC, sensor_id);

    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "name", MQTT_DEVICE_NAME);
    cJSON_AddStringToObject(device, "identifiers", config.deviceName);
    cJSON_AddStringToObject(device, "manufacturer", MQTT_DEVICE_MANUFACTURER);
    cJSON_AddStringToObject(device, "model", MQTT_DEVICE_MODEL);

    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "name", name);
    cJSON_AddStringToObject(payload, "state_topic", state_topic);
    char unique_id[64];
    snprintf(unique_id, sizeof(unique_id), "%s_%s", config.deviceName, sensor_id);
    cJSON_AddStringToObject(payload, "unique_id", unique_id);
    cJSON_AddItemToObject(payload, "device", device);

    if (device_class != NULL) {
        cJSON_AddStringToObject(payload, "device_class", device_class);
    }
    if (unit != NULL) {
        cJSON_AddStringToObject(payload, "unit_of_measurement", unit);
    }

    char *payload_str = cJSON_PrintUnformatted(payload);
    ha_mqtt_publish(topic, payload_str, 1, 1);

    cJSON_Delete(payload);
    free(payload_str);

    ESP_LOGI(TAG, "发布发现配置: %s", sensor_id);
}

void sensorTask(void *param) {
    const TickType_t interval = pdMS_TO_TICKS(MQTT_SENSOR_UPDATE_INTERVAL_MS);
    const TickType_t initial_delay = pdMS_TO_TICKS(5000);

    ESP_LOGI(TAG, "等待%d秒后开始传感器任务...", initial_delay / 1000);
    vTaskDelay(initial_delay);

    if (!g_sensor_running) {
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "发布传感器发现配置...");
    ha_sensor_publish_discovery();

    while (g_sensor_running) {
        ESP_LOGI(TAG, "等待下一次传感器读取...");

        vTaskDelay(interval);

        if (!g_sensor_running) {
            break;
        }

        if (!ha_mqtt_is_connected()) {
            ESP_LOGW(TAG, "MQTT未连接，跳过本次上报");
            continue;
        }

        extern esp_err_t sensor_init(void);
        extern esp_err_t sensor_read_data(as7341_channels_spectral_data_t *data);
        extern void sensor_deinit(void);

        esp_err_t ret = sensor_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "传感器初始化失败");
            continue;
        }

        as7341_channels_spectral_data_t sensor_data;
        ret = sensor_read_data(&sensor_data);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "传感器读取失败");
            sensor_deinit();
            continue;
        }

        ha_sensor_publish_data(&sensor_data);
        sensor_deinit();

        scd41_data_t scd_data;
        ret = scd41_read_data(&scd_data);
        if (ret == ESP_OK && scd_data.data_valid) {
            ha_sensor_publish_scd41_data(&scd_data);
        } else {
            ESP_LOGW(TAG, "SCD41数据读取失败，跳过本次上报");
        }

        ESP_LOGI(TAG, "传感器数据已上报");
    }

    g_sensor_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t ha_sensor_start(void) {
    if (g_sensor_running) {
        ESP_LOGW(TAG, "传感器任务已在运行");
        return ESP_OK;
    }

    g_sensor_running = true;

    BaseType_t ret = xTaskCreate(sensorTask, "ha_sensor", 8192, NULL, 4, &g_sensor_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "传感器任务创建失败");
        g_sensor_running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "传感器任务已启动");
    return ESP_OK;
}

esp_err_t ha_sensor_stop(void) {
    if (!g_sensor_running) {
        return ESP_OK;
    }

    g_sensor_running = false;

    if (g_sensor_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "传感器任务已停止");
    return ESP_OK;
}

esp_err_t ha_sensor_publish_discovery(void) {
    device_config_t config;
    if (configLoad(&config) != ESP_OK) {
        ESP_LOGE(TAG, "加载配置失败");
        return ESP_FAIL;
    }

    publishSensorDiscovery("f1", "Plant F1 (415-445nm)", NULL, NULL);
    publishSensorDiscovery("f2", "Plant F2 (445-480nm)", NULL, NULL);
    publishSensorDiscovery("f3", "Plant F3 (480-520nm)", NULL, NULL);
    publishSensorDiscovery("f4", "Plant F4 (520-550nm)", NULL, NULL);
    publishSensorDiscovery("f5", "Plant F5 (550-585nm)", NULL, NULL);
    publishSensorDiscovery("f6", "Plant F6 (585-630nm)", NULL, NULL);
    publishSensorDiscovery("f7", "Plant F7 (630-680nm)", NULL, NULL);
    publishSensorDiscovery("f8", "Plant F8 (680-720nm)", NULL, NULL);
    publishSensorDiscovery("clear", "Plant Clear Channel", NULL, NULL);
    publishSensorDiscovery("nir", "Plant NIR", NULL, NULL);

    publishSensorDiscovery("ndvi", "Plant NDVI", NULL, NULL);
    publishSensorDiscovery("sipi", "Plant SIPI", NULL, NULL);
    publishSensorDiscovery("psri", "Plant PSRI", NULL, NULL);
    publishSensorDiscovery("cri550", "Plant CRI550", NULL, NULL);
    publishSensorDiscovery("cri700", "Plant CRI700", NULL, NULL);

    publishSensorDiscovery("co2", "CO2 Concentration", "carbon_dioxide", "ppm");
    publishSensorDiscovery("temperature", "Temperature", "temperature", "°C");
    publishSensorDiscovery("humidity", "Humidity", "humidity", "%");

    ESP_LOGI(TAG, "所有传感器发现配置已发布");
    return ESP_OK;
}

esp_err_t ha_sensor_publish_data(as7341_channels_spectral_data_t *data) {
    if (data == NULL) {
        ESP_LOGE(TAG, "无效的传感器数据");
        return ESP_ERR_INVALID_ARG;
    }

    device_config_t config;
    if (configLoad(&config) != ESP_OK) {
        return ESP_FAIL;
    }

    char topic[64];
    char value_str[32];

    snprintf(topic, sizeof(topic), "%s/sensor/f1", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%u", data->f1);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/f2", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%u", data->f2);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/f3", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%u", data->f3);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/f4", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%u", data->f4);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/f5", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%u", data->f5);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/f6", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%u", data->f6);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/f7", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%u", data->f7);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/f8", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%u", data->f8);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/clear", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%u", data->clear);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/nir", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%u", data->nir);
    ha_mqtt_publish(topic, value_str, 1, 1);

    float ndvi = 0.0f;
    if ((data->f8 + data->f5) != 0) {
        ndvi = (float)(data->f8 - data->f5) / (float)(data->f8 + data->f5);
    }

    snprintf(topic, sizeof(topic), "%s/sensor/ndvi", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%.4f", ndvi);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/sipi", MQTT_SENSOR_BASE_TOPIC);
    float sipi = 0.0f;
    if ((data->nir - data->f7) != 0) {
        sipi = (float)(data->nir - data->f2) / (float)(data->nir - data->f7);
    }
    snprintf(value_str, sizeof(value_str), "%.4f", sipi);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/psri", MQTT_SENSOR_BASE_TOPIC);
    float psri = 0.0f;
    if (data->f8 != 0) {
        psri = (float)(data->f7 - data->f5) / (float)data->f8;
    }
    snprintf(value_str, sizeof(value_str), "%.4f", psri);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/cri550", MQTT_SENSOR_BASE_TOPIC);
    float cri550 = 0.0f;
    if (data->f3 != 0 && data->f5 != 0) {
        cri550 = (1.0f / (float)data->f3) - (1.0f / (float)data->f5);
    }
    snprintf(value_str, sizeof(value_str), "%.6f", cri550);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/cri700", MQTT_SENSOR_BASE_TOPIC);
    float cri700 = 0.0f;
    if (data->f3 != 0 && data->f8 != 0) {
        cri700 = (1.0f / (float)data->f3) - (1.0f / (float)data->f8);
    }
    snprintf(value_str, sizeof(value_str), "%.6f", cri700);
    ha_mqtt_publish(topic, value_str, 1, 1);

    ESP_LOGI(TAG, "传感器数据发布完成");
    return ESP_OK;
}

esp_err_t ha_sensor_publish_scd41_data(const scd41_data_t *data) {
    if (data == NULL || !data->data_valid) {
        ESP_LOGE(TAG, "无效的SCD41传感器数据");
        return ESP_ERR_INVALID_ARG;
    }

    device_config_t config;
    if (configLoad(&config) != ESP_OK) {
        return ESP_FAIL;
    }

    char topic[64];
    char value_str[32];

    snprintf(topic, sizeof(topic), "%s/sensor/co2", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%u", data->co2_ppm);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/temperature", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%.2f", data->temperature_c);
    ha_mqtt_publish(topic, value_str, 1, 1);

    snprintf(topic, sizeof(topic), "%s/sensor/humidity", MQTT_SENSOR_BASE_TOPIC);
    snprintf(value_str, sizeof(value_str), "%.2f", data->humidity_rh);
    ha_mqtt_publish(topic, value_str, 1, 1);

    ESP_LOGI(TAG, "SCD41传感器数据发布完成");
    return ESP_OK;
}
