#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/app_state.h"
#include "core/event_bus.h"
#include "storage/storage.h"
#include "network/wifi_sta.h"
#include "network/mqtt_wrapper.h"
#include "service/ha_integration.h"
#include "service/ai_service.h"
#include "service/sensor_service.h"
#include "driver/driver_scd41.h"
#include "task_network.h"

static const char *TAG = "TASK_NET";

void taskNetwork(void *param)
{
    ESP_LOGI(TAG, "网络连接任务启动");
    appStateSet(APP_STATE_CONNECTING);

    device_config_t config;
    esp_err_t ret = storageLoad(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配置加载失败");
        appStateSet(APP_STATE_ERROR);
        vTaskDelete(NULL);
        return;
    }

    ret = wifiStaConnect(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi连接初始化失败");
        appStateSet(APP_STATE_ERROR);
        vTaskDelete(NULL);
        return;
    }

    if (!wifiStaWaitConnected(30000)) {
        ESP_LOGE(TAG, "WiFi连接超时，网络任务退出");
        appStateSet(APP_STATE_ERROR);
        vTaskDelete(NULL);
        return;
    }

    ret = mqttClientInit();
    if (ret == ESP_OK) {
        mqttClientConnect();
    }

    haIntegrationStart();
    haIntegrationSetAiCallback(aiServiceRun);

    sensorServiceStart();

    esp_err_t scdRet = scd41Init();
    if (scdRet != ESP_OK) {
        ESP_LOGW(TAG, "SCD41初始化失败，CO2/温湿度传感器不可用");
    } else {
        ESP_LOGI(TAG, "SCD41初始化成功");
    }

    appStateSet(APP_STATE_RUNNING);
    ESP_LOGI(TAG, "网络连接完成，系统进入运行状态");

    aiServiceRun();

    vTaskDelete(NULL);
}
