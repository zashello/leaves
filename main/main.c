#include <stdio.h>
#include "hardware/servo.h"
#include "hardware/button.h"
#include <as7341.h>
#include "freertos/FreeRTOS.h"
#include "esp_app_trace.h"
#include "esp_log.h"
#include "hardware/sensor.h"
#include "http/model_api.h"
#include "wifi/wifi_connect.h"
#include "wifi/provision/provision.h"
#include "config/nvs_config.h"

static const char *TAG = "APP";

static void buttonEventHandler(button_event_t event)
{
    if (event == BUTTON_EVENT_LONG_PRESS) {
        ESP_LOGW(TAG, "长按3秒触发，清除配网参数并重启...");
        configClear();
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
}

void servo(void *param)
{
    ESP_LOGI(TAG, "Starting leaves application");

    esp_err_t ret = servo_init(LEDC_CHANNEL_0, GPIO_NUM_18);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize servo motor: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Motor initialized successfully");

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Testing forward rotation for 3 seconds...");
    servo_forward_for_time(3000);

    vTaskDelay(pdMS_TO_TICKS(5000));

    ESP_LOGI(TAG, "Testing different speeds...");
    servo_set_speed(0.5f);
    vTaskDelay(pdMS_TO_TICKS(2000));

    servo_set_speed(-0.5f);
    vTaskDelay(pdMS_TO_TICKS(2000));

    servo_stop();
    ESP_LOGI(TAG, "Motor stopped");

    servo_deinit();
    ESP_LOGI(TAG, "Application completed");
}

void aiTask(void *param)
{
    ESP_LOGI(TAG, "启动AI分析任务");
    if (!wifiWaitConnected(30000)) {
        ESP_LOGE(TAG, "WiFi连接超时，AI任务退出");
        vTaskDelete(NULL);
        return;
    }
    aiTest();
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Leaves 植物分析系统启动");

    esp_err_t ret = configInit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配置系统初始化失败");
        return;
    }

    ret = buttonInit();
    if (ret == ESP_OK) {
        buttonRegisterCallback(buttonEventHandler);
    } else {
        ESP_LOGW(TAG, "按键初始化失败，重置功能不可用");
    }

    if (configIsValid()) {
        ESP_LOGI(TAG, "检测到已保存配置，使用配置连接WiFi");

        device_config_t config;
        ret = configLoad(&config);
        if (ret == ESP_OK) {
            ret = wifiConnectWithConfig(&config);
            if (ret == ESP_OK) {
                xTaskCreate(aiTask, "ai", 8192, NULL, 2, NULL);
            } else {
                ESP_LOGE(TAG, "WiFi连接初始化失败，进入配网模式");
                provisionStart();
            }
        } else {
            ESP_LOGE(TAG, "配置加载失败，进入配网模式");
            provisionStart();
        }
    } else {
        ESP_LOGI(TAG, "未检测到有效配置，启动配网模式");
        provisionStart();
    }
}
