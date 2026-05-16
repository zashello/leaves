#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage/storage.h"
#include "network/wifi_sta.h"
#include "network/mqtt_wrapper.h"
#include "service/sensor_service.h"
#include "service/ha_integration.h"
#include "service/ai_service.h"
#include "system/system_log.h"
#include "driver/driver_scd41.h"

static const char *TAG = "AUTO_NET";

void taskAutoNetwork(void *param)
{
    device_config_t config;
    esp_err_t ret = storageLoad(&config);
    if (ret != ESP_OK) {
        systemLogAdd(LOG_LEVEL_ERROR, "AUTO NET: LOAD CONFIG FAIL");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "AUTO CONNECT WIFI: %s", config.wifiSsid);

    ret = wifiStaConnect(&config);
    if (ret != ESP_OK) {
        systemLogAdd(LOG_LEVEL_ERROR, "AUTO NET: WIFI INIT FAIL");
        vTaskDelete(NULL);
        return;
    }

    if (!wifiStaWaitConnected(30000)) {
        systemLogAdd(LOG_LEVEL_WARNING, "AUTO NET: WIFI TIMEOUT");
        vTaskDelete(NULL);
        return;
    }

    systemLogAdd(LOG_LEVEL_INFO, "AUTO NET: WIFI CONNECTED");

    if (config.enableMqtt) {
        mqttClientInit();
        mqttClientConnect();
        haIntegrationStart();
        haIntegrationSetAiCallback(aiServiceRun);
        haIntegrationSetSensorCallback(sensorServiceReportOnce);
        sensorServiceStart();
        systemLogAdd(LOG_LEVEL_INFO, "AUTO NET: MQTT STARTED");
    }

    if (config.enableAiService) {
        systemLogAdd(LOG_LEVEL_INFO, "AUTO NET: AI SERVICE READY");
    }

    vTaskDelete(NULL);
}
