#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "core/app_state.h"
#include "core/task_manager.h"
#include "core/event_bus.h"
#include "storage/storage.h"
#include "driver/driver_button.h"
#include "driver/driver_scd41.h"
#include "system/system_log.h"
#include "system/menu_system.h"
#include "system/menu_display.h"
#include "service/display_service.h"
#include "network/wifi_sta.h"
#include "network/mqtt_wrapper.h"
#include "service/sensor_service.h"

static const char *TAG = "APP";

static void menuButtonHandler(button_event_t event)
{
    if (event == BUTTON_EVENT_LONG_PRESS) {
        ESP_LOGW(TAG, "LONG PRESS: CLEAR CONFIG AND REBOOT");
        storageClear();
        systemLogAdd(LOG_LEVEL_CRITICAL, "LONG PRESS RESET");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
        return;
    }
    menuSystemHandleEvent(event);
}

static void onStateChange(app_state_t from, app_state_t to)
{
    ESP_LOGI(TAG, "STATE: %s -> %s", appStateGetName(from), appStateGetName(to));
}

void app_main(void)
{
    ESP_LOGI(TAG, "LEAVES PLANT MONITOR START");

    appStateInit();
    appStateRegisterCallback(onStateChange);
    appStateSet(APP_STATE_INIT);

    eventBusInit();

    esp_err_t ret = storageInit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "STORAGE INIT FAIL");
        appStateSet(APP_STATE_ERROR);
        return;
    }

    systemLogInit();
    systemLogAdd(LOG_LEVEL_INFO, "SYSTEM BOOT");

    ret = scd41Init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SCD41 INIT FAIL");
        systemLogAdd(LOG_LEVEL_WARNING, "SCD41 INIT FAIL");
    } else {
        systemLogAdd(LOG_LEVEL_INFO, "SCD41 INIT OK");
    }

    ret = displayServiceInit();
    if (ret == ESP_OK) {
        displayServiceShowInitScreen();
        systemLogAdd(LOG_LEVEL_INFO, "OLED INIT OK");
    } else {
        ESP_LOGW(TAG, "OLED INIT FAIL");
        systemLogAdd(LOG_LEVEL_WARNING, "OLED INIT FAIL");
    }

    if (buttonInitMulti() == ESP_OK) {
        buttonRegisterCallback(menuButtonHandler);
        systemLogAdd(LOG_LEVEL_INFO, "BUTTON INIT OK");
    } else {
        ESP_LOGW(TAG, "BUTTON INIT FAIL");
        systemLogAdd(LOG_LEVEL_WARNING, "BUTTON INIT FAIL");
    }

    eiClassifierInit();
    systemLogAdd(LOG_LEVEL_INFO, "EI CLASSIFIER INIT OK");

    menuSystemInit();
    systemLogAdd(LOG_LEVEL_INFO, "MENU SYSTEM READY");

    // 自动联网逻辑
    device_config_t autoConfig;
    memset(&autoConfig, 0, sizeof(autoConfig));
    
    if (storageLoad(&autoConfig) == ESP_OK && autoConfig.configValid) {
        if (autoConfig.enableAutoNetwork) {
            ESP_LOGI(TAG, "AUTO CONNECT WIFI");
            systemLogAdd(LOG_LEVEL_INFO, "AUTO CONNECT WIFI");
            
            menuDisplayShowWaiting("AUTO CONNECT...");
            
            esp_err_t wifiResult = wifiStaConnect(&autoConfig);
            if (wifiResult == ESP_OK) {
                bool connected = wifiStaWaitConnected(30000);
                if (connected) {
                    systemLogAdd(LOG_LEVEL_INFO, "WIFI CONNECTED");
                    menuDisplayShowSuccess("WIFI OK");
                    
                    if (autoConfig.enableMqtt) {
                        mqttClientInit();
                        mqttClientConnect();
                        sensorServiceStart();
                        systemLogAdd(LOG_LEVEL_INFO, "MQTT STARTED");
                    }
                    
                    vTaskDelay(pdMS_TO_TICKS(1000));
                } else {
                    systemLogAdd(LOG_LEVEL_ERROR, "WIFI CONNECT TIMEOUT");
                    menuDisplayShowError("WIFI TIMEOUT");
                    vTaskDelay(pdMS_TO_TICKS(1500));
                }
            } else {
                systemLogAdd(LOG_LEVEL_ERROR, "WIFI INIT FAIL");
                menuDisplayShowError("WIFI INIT FAIL");
                vTaskDelay(pdMS_TO_TICKS(1500));
            }
        }
    }

    appStateSet(APP_STATE_RUNNING);
    ESP_LOGI(TAG, "ENTER MENU LOOP");

    while (1) {
        menuSystemShow();
        menuSystemCheckTimeout();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
