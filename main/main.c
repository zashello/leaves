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
#include "driver/driver_as7341.h"
#include "system/system_log.h"
#include "system/menu_system.h"
#include "system/menu_display.h"
#include "service/display_service.h"
#include "network/wifi_sta.h"
#include "network/mqtt_wrapper.h"
#include "service/sensor_service.h"
#include "service/ha_integration.h"
#include "service/ai_service.h"

static const char *TAG = "APP";

static void printMqttIntegrationStatus(void)
{
    ESP_LOGI(TAG, "========== MQTT集成状态诊断 ==========");
    ESP_LOGI(TAG, "MQTT连接状态: %s", mqttClientIsConnected() ? "已连接" : "未连接");
    ESP_LOGI(TAG, "MQTT连接状态: %d", (int)mqttClientGetState());
    ESP_LOGI(TAG, "HomeAssistant集成: 已启动");
    ESP_LOGI(TAG, "AI分析回调: 已设置 (aiServiceRun)");
    ESP_LOGI(TAG, "传感器上报回调: 已设置 (sensorServiceReportOnce)");
    ESP_LOGI(TAG, "支持的主题:");
    ESP_LOGI(TAG, "  - %s (AI触发)", MQTT_TRIGGER_TOPIC);
    ESP_LOGI(TAG, "  - %s (设备命令)", MQTT_COMMAND_TOPIC);
    ESP_LOGI(TAG, "支持的指令:");
    ESP_LOGI(TAG, "  - ai_analysis");
    ESP_LOGI(TAG, "  - sensor_report");
    ESP_LOGI(TAG, "===========================================");
}

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

    // AS7341初始化 - 配置LED控制
    ret = as7341Init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AS7341 INIT FAIL: %s", esp_err_to_name(ret));
        systemLogAdd(LOG_LEVEL_ERROR, "AS7341 INIT FAIL");
        appStateSet(APP_STATE_ERROR);
        return;
    } else {
        systemLogAdd(LOG_LEVEL_INFO, "AS7341 INIT OK");
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
                        ESP_LOGI(TAG, "初始化MQTT和传感器服务...");
                        
                        // MQTT初始化
                        esp_err_t mqttInitRet = mqttClientInit();
                        if (mqttInitRet != ESP_OK) {
                            ESP_LOGE(TAG, "MQTT初始化失败: %s", esp_err_to_name(mqttInitRet));
                            systemLogAdd(LOG_LEVEL_ERROR, "MQTT INIT FAIL");
                        } else {
                            ESP_LOGI(TAG, "MQTT初始化成功");
                            systemLogAdd(LOG_LEVEL_INFO, "MQTT INIT OK");
                        }

                        // MQTT连接
                        esp_err_t mqttConnRet = mqttClientConnect();
                        if (mqttConnRet != ESP_OK) {
                            ESP_LOGE(TAG, "MQTT连接失败: %s", esp_err_to_name(mqttConnRet));
                            systemLogAdd(LOG_LEVEL_ERROR, "MQTT CONNECT FAIL");
                        } else {
                            ESP_LOGI(TAG, "MQTT连接指令已发送");
                            
                            // 等待MQTT连接建立（最多10秒）
                            int connectRetry = 0;
                            bool mqttConnected = false;
                            while (connectRetry < 10 && !mqttConnected) {
                                mqttConnected = mqttClientIsConnected();
                                if (mqttConnected) {
                                    break;
                                }
                                vTaskDelay(pdMS_TO_TICKS(1000));
                                connectRetry++;
                                ESP_LOGI(TAG, "等待MQTT连接... (%d/10)", connectRetry);
                            }
                            
                            if (mqttConnected) {
                                ESP_LOGI(TAG, "MQTT连接确认成功");
                                systemLogAdd(LOG_LEVEL_INFO, "MQTT CONNECTED OK");

                                // 启动HomeAssistant集成（关键修复）
                                ESP_LOGI(TAG, "启动HomeAssistant集成...");
                                haIntegrationStart();
                                systemLogAdd(LOG_LEVEL_INFO, "HA INTEGRATION STARTED");
                                
                                //设置AI分析回调（关键修复）
                                ESP_LOGI(TAG, "设置AI分析回调...");
                                haIntegrationSetAiCallback(aiServiceRun);
                                ESP_LOGI(TAG, "设置传感器上报回调...");
                                haIntegrationSetSensorCallback(sensorServiceReportOnce);
                                systemLogAdd(LOG_LEVEL_INFO, "MQTT CALLBACKS SET");
                                
                                ESP_LOGI(TAG, "✓ MQTT指令处理功能已启用");
                                systemLogAdd(LOG_LEVEL_INFO, "MQTT COMMAND HANDLER READY");
                                printMqttIntegrationStatus();  // 调用诊断函数
                            } else {
                                ESP_LOGW(TAG, "MQTT连接超时（10秒），后台将继续重连");
                                systemLogAdd(LOG_LEVEL_WARNING, "MQTT CONNECT TIMEOUT, WILL RETRY");
                                // 即使MQTT连接超时，也启用HA集成以便后续重连后能接收消息
                                haIntegrationStart();
                                haIntegrationSetAiCallback(aiServiceRun);
                                haIntegrationSetSensorCallback(sensorServiceReportOnce);
                                printMqttIntegrationStatus();  // 调用诊断函数
                                printMqttIntegrationStatus();  // 调用诊断函数
                            }
                        }

                        // 传感器服务启动（独立于MQTT连接状态）
                        esp_err_t sensorRet = sensorServiceStart();
                        if (sensorRet != ESP_OK) {
                            ESP_LOGE(TAG, "传感器服务启动失败: %s", esp_err_to_name(sensorRet));
                            systemLogAdd(LOG_LEVEL_ERROR, "SENSOR SERVICE START FAIL");
                        } else {
                            ESP_LOGI(TAG, "传感器服务启动成功");
                            systemLogAdd(LOG_LEVEL_INFO, "SENSOR SERVICE STARTED");
                        }
                        
                        systemLogAdd(LOG_LEVEL_INFO, "AUTO NETWORK SETUP COMPLETE");
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
