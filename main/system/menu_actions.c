#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "app_config.h"
#include "menu_actions.h"
#include "menu_system.h"
#include "menu_display.h"
#include "driver/driver_scd41.h"
#include "driver/driver_as7341.h"
#include "driver/ei_classifier.h"
#include "storage/storage.h"
#include "system/system_log.h"
#include "network/wifi_sta.h"
#include "network/provision/prov_main.h"
#include "network/mqtt_wrapper.h"
#include "service/ha_integration.h"
#include "service/sensor_service.h"
#include "service/ai_service.h"
#include "service/notify.h"

static const char *TAG = "MENU_ACT";

static void executeScd41Read(void *param)
{
    scd41_data_t data;
    esp_err_t ret = scd41ReadData(&data);

    if (ret == ESP_OK && data.data_valid) {
        char msg[64];
        snprintf(msg, sizeof(msg), "SCD41 CO2=%u T=%.1f H=%.1f",
                 data.co2_ppm, data.temperature_c, data.humidity_rh);
        systemLogAdd(LOG_LEVEL_INFO, msg);

        menuDisplayShowScd41Data(&data);
        menuSystemGetContext()->state = MENU_STATE_DATA_VIEW;
    } else {
        systemLogAdd(LOG_LEVEL_ERROR, "SCD41 READ FAILED");
        menuDisplayShowError("SENSOR FAIL");
        vTaskDelay(pdMS_TO_TICKS(2000));
        menuSystemExitWaiting();
        menuSystemShow();
    }

    vTaskDelete(NULL);
}

void actionShowScd41Data(void)
{
    menuSystemEnterWaiting("READ SCD41...");
    systemLogAdd(LOG_LEVEL_INFO, "TRIGGER SCD41 READ");

    xTaskCreate(executeScd41Read, "scd41_read", 4096, NULL, 5, NULL);
}

static void executePlantAnalysis(void *param)
{
    menuDisplayShowWaiting("INIT AS7341...");

    esp_err_t ret = as7341Init();
    if (ret != ESP_OK) {
        systemLogAdd(LOG_LEVEL_ERROR, "AS7341 INIT FAIL");
        menuDisplayShowError("AS7341 FAIL");
        vTaskDelay(pdMS_TO_TICKS(2000));
        menuSystemExitWaiting();
        menuSystemShow();
        vTaskDelete(NULL);
        return;
    }

    menuDisplayShowWaiting("READ SPECTRAL...");

    as7341_channels_spectral_data_t spectralData;
    ret = as7341ReadData(&spectralData);
    as7341Deinit();

    if (ret != ESP_OK) {
        systemLogAdd(LOG_LEVEL_ERROR, "AS7341 READ FAIL");
        menuDisplayShowError("AS7341 FAIL");
        vTaskDelay(pdMS_TO_TICKS(2000));
        menuSystemExitWaiting();
        menuSystemShow();
        vTaskDelete(NULL);
        return;
    }

    menuDisplayShowWaiting("AI ANALYSIS...");

    ei_inference_result_t inferResult;
    ret = eiClassifierRun(&spectralData, &inferResult);

    if (ret == ESP_OK) {
        char msg[64];
        snprintf(msg, sizeof(msg), "RESULT: %s %.1f%%",
                 inferResult.bestLabel, inferResult.bestValue * 100.0f);
        systemLogAdd(LOG_LEVEL_INFO, msg);

        menuDisplayShowPlantAnalysis(&inferResult);
        menuSystemGetContext()->state = MENU_STATE_DATA_VIEW;
    } else {
        systemLogAdd(LOG_LEVEL_ERROR, "EI CLASSIFY FAIL");
        menuDisplayShowError("AI FAIL");
        vTaskDelay(pdMS_TO_TICKS(2000));
        menuSystemExitWaiting();
        menuSystemShow();
    }

    vTaskDelete(NULL);
}

void actionShowPlantAnalysis(void)
{
    menuSystemEnterWaiting("PLANT ANALYSIS...");
    systemLogAdd(LOG_LEVEL_INFO, "TRIGGER PLANT ANALYSIS");

    xTaskCreate(executePlantAnalysis, "plant_analysis", 8192, NULL, 5, NULL);
}

void actionShowAbout(void)
{
    menuDisplayShowAbout();
    menuSystemGetContext()->state = MENU_STATE_DATA_VIEW;
}

static void executeShowLog(void *param)
{
    int count = systemLogGetCount();

    if (count == 0) {
        menuDisplayShowMessage("NO LOG");
        vTaskDelay(pdMS_TO_TICKS(2000));
        menuSystemExitWaiting();
        menuSystemShow();
        vTaskDelete(NULL);
        return;
    }

    log_entry_t entry;
    systemLogGetEntry(count - 1, &entry);
    menuDisplayShowLogEntry(&entry, count - 1, count);
    menuSystemGetContext()->state = MENU_STATE_DATA_VIEW;

    vTaskDelete(NULL);
}

void actionShowLog(void)
{
    menuSystemEnterWaiting("LOADING LOG...");
    xTaskCreate(executeShowLog, "show_log", 4096, NULL, 5, NULL);
}

static void executeClearLog(void *param)
{
    systemLogClear();
    menuDisplayShowSuccess("LOG CLEARED");
    vTaskDelay(pdMS_TO_TICKS(1500));
    menuSystemExitWaiting();
    menuSystemShow();
    vTaskDelete(NULL);
}

void actionClearLog(void)
{
    menuSystemEnterWaiting("CLEARING...");
    xTaskCreate(executeClearLog, "clear_log", 4096, NULL, 5, NULL);
}

static void doFactoryReset(void)
{
    storageClear();
    systemLogAdd(LOG_LEVEL_CRITICAL, "FACTORY RESET");
    menuDisplayShowMessage("REBOOTING...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}

void actionFactoryReset(void)
{
    menuDisplayShowConfirm("FACTORY RESET?");
    menuSystemGetContext()->state = MENU_STATE_CONFIRM;
    menuSystemGetContext()->confirmAction = doFactoryReset;
}

static void doReboot(void)
{
    systemLogAdd(LOG_LEVEL_INFO, "REBOOT");
    menuDisplayShowMessage("REBOOTING...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}

void actionReboot(void)
{
    menuDisplayShowConfirm("REBOOT DEVICE?");
    menuSystemGetContext()->state = MENU_STATE_CONFIRM;
    menuSystemGetContext()->confirmAction = doReboot;
}

static void executeProvision(void *param)
{
    esp_err_t ret = provStart();
    if (ret == ESP_OK) {
        menuDisplayShowWifiAp(PROVISION_SSID, "192.168.4.1");
        menuSystemGetContext()->state = MENU_STATE_DATA_VIEW;
        systemLogAdd(LOG_LEVEL_INFO, "PROVISION STARTED");
    } else {
        menuDisplayShowError("PROVISION FAIL");
        vTaskDelay(pdMS_TO_TICKS(2000));
        menuSystemExitWaiting();
        menuSystemShow();
        systemLogAdd(LOG_LEVEL_ERROR, "PROVISION START FAIL");
    }

    vTaskDelete(NULL);
}

void actionStartProvision(void)
{
    menuSystemEnterWaiting("STARTING AP...");
    xTaskCreate(executeProvision, "provision", 8192, NULL, 5, NULL);
}

static void executeConnectWifi(void *param)
{
    device_config_t config;
    esp_err_t ret = storageLoad(&config);
    if (ret != ESP_OK || !config.configValid) {
        menuDisplayShowError("NO CONFIG");
        vTaskDelay(pdMS_TO_TICKS(2000));
        menuSystemExitWaiting();
        menuSystemShow();
        vTaskDelete(NULL);
        return;
    }

    menuDisplayShowWaiting("CONNECTING...");

    ret = wifiStaConnect(&config);
    if (ret != ESP_OK) {
        menuDisplayShowError("WIFI INIT FAIL");
        vTaskDelay(pdMS_TO_TICKS(2000));
        menuSystemExitWaiting();
        menuSystemShow();
        vTaskDelete(NULL);
        return;
    }

    if (wifiStaWaitConnected(30000)) {
        menuDisplayShowSuccess("WIFI OK");
        systemLogAdd(LOG_LEVEL_INFO, "WIFI CONNECTED");

        if (config.enableMqtt) {
            mqttClientInit();
            mqttClientConnect();
            sensorServiceStart();
            systemLogAdd(LOG_LEVEL_INFO, "MQTT STARTED");
        }

        vTaskDelay(pdMS_TO_TICKS(1500));
        menuSystemExitWaiting();
        menuSystemShow();
    } else {
        menuDisplayShowError("WIFI TIMEOUT");
        systemLogAdd(LOG_LEVEL_ERROR, "WIFI CONNECT TIMEOUT");
        vTaskDelay(pdMS_TO_TICKS(2000));
        menuSystemExitWaiting();
        menuSystemShow();
    }

    vTaskDelete(NULL);
}

void actionConnectWifi(void)
{
    menuSystemEnterWaiting("WIFI CONNECT...");
    xTaskCreate(executeConnectWifi, "wifi_conn", 8192, NULL, 5, NULL);
}

static void executeDisconnectWifi(void *param)
{
    esp_wifi_stop();
    systemLogAdd(LOG_LEVEL_INFO, "WIFI DISCONNECTED");
    menuDisplayShowSuccess("WIFI OFF");
    vTaskDelay(pdMS_TO_TICKS(1500));
    menuSystemExitWaiting();
    menuSystemShow();
    vTaskDelete(NULL);
}

void actionDisconnectWifi(void)
{
    menuSystemEnterWaiting("DISCONNECT...");
    xTaskCreate(executeDisconnectWifi, "wifi_disc", 4096, NULL, 5, NULL);
}

void actionMqttStatus(void)
{
    if (mqttClientIsConnected()) {
        menuDisplayShowMessage("MQTT: CONNECTED");
    } else {
        menuDisplayShowMessage("MQTT: OFFLINE");
    }
    menuSystemGetContext()->state = MENU_STATE_DATA_VIEW;
}

static void executeUpload(void *param)
{
    sensorServiceReportOnce();
    systemLogAdd(LOG_LEVEL_INFO, "MANUAL SENSOR UPLOAD");
    menuDisplayShowSuccess("UPLOAD OK");
    vTaskDelay(pdMS_TO_TICKS(1500));
    menuSystemExitWaiting();
    menuSystemShow();
    vTaskDelete(NULL);
}

void actionTriggerUpload(void)
{
    if (!mqttClientIsConnected()) {
        menuDisplayShowError("MQTT OFFLINE");
        vTaskDelay(pdMS_TO_TICKS(2000));
        menuSystemGetContext()->state = MENU_STATE_READY;
        return;
    }
    menuSystemEnterWaiting("UPLOADING...");
    xTaskCreate(executeUpload, "upload", 4096, NULL, 5, NULL);
}

static void executeAi(void *param)
{
    aiServiceRun();
    systemLogAdd(LOG_LEVEL_INFO, "AI ANALYSIS DONE");
    menuDisplayShowSuccess("AI DONE");
    vTaskDelay(pdMS_TO_TICKS(1500));
    menuSystemExitWaiting();
    menuSystemShow();
    vTaskDelete(NULL);
}

void actionTriggerAi(void)
{
    if (!wifiStaIsConnected()) {
        menuDisplayShowError("WIFI OFFLINE");
        vTaskDelay(pdMS_TO_TICKS(2000));
        menuSystemGetContext()->state = MENU_STATE_READY;
        return;
    }
    menuSystemEnterWaiting("AI ANALYSIS...");
    xTaskCreate(executeAi, "ai_task", 12288, NULL, 5, NULL);
}
