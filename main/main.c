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
#include "tasks/task_provision.h"
#include "tasks/task_network.h"
#include "service/display_service.h"

static const char *TAG = "APP";

static void buttonEventHandler(button_event_t event)
{
    if (event == BUTTON_EVENT_LONG_PRESS) {
        ESP_LOGW(TAG, "长按3秒触发，清除配网参数并重启...");
        storageClear();
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
}

static void onStateChange(app_state_t from, app_state_t to)
{
    ESP_LOGI(TAG, "系统状态变更: %s -> %s", appStateGetName(from), appStateGetName(to));
}

static void onEvent(app_event_t event, void *data)
{
    switch (event) {
        case EVENT_WIFI_CONNECTED:
            ESP_LOGI(TAG, "[事件] WiFi已连接");
            break;
        case EVENT_WIFI_DISCONNECTED:
            ESP_LOGW(TAG, "[事件] WiFi已断开");
            break;
        case EVENT_MQTT_CONNECTED:
            ESP_LOGI(TAG, "[事件] MQTT已连接");
            break;
        case EVENT_MQTT_DISCONNECTED:
            ESP_LOGW(TAG, "[事件] MQTT已断开");
            break;
        case EVENT_BUTTON_LONG_PRESS:
            ESP_LOGW(TAG, "[事件] 按键长按");
            break;
        default:
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Leaves 植物分析系统启动");

    appStateInit();
    appStateRegisterCallback(onStateChange);

    eventBusInit();
    eventBusSubscribe(EVENT_WIFI_CONNECTED, onEvent);
    eventBusSubscribe(EVENT_WIFI_DISCONNECTED, onEvent);
    eventBusSubscribe(EVENT_MQTT_CONNECTED, onEvent);
    eventBusSubscribe(EVENT_MQTT_DISCONNECTED, onEvent);

    esp_err_t ret = storageInit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "存储系统初始化失败");
        appStateSet(APP_STATE_ERROR);
        return;
    }

    ret = buttonInit();
    if (ret == ESP_OK) {
        buttonRegisterCallback(buttonEventHandler);
    } else {
        ESP_LOGW(TAG, "按键初始化失败，重置功能不可用");
    }

    taskManagerInit();

    ret = displayServiceInit();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OLED显示服务初始化成功");
        displayServiceShowInitScreen();
    } else {
        ESP_LOGW(TAG, "OLED显示服务初始化失败，继续运行");
    }

    if (storageIsValid()) {
        ESP_LOGI(TAG, "检测到已保存配置，启动网络连接任务");
        task_config_t networkTask = {
            .name = "network",
            .function = taskNetwork,
            .stackSize = 8192,
            .priority = 5,
            .param = NULL
        };
        taskManagerCreate(&networkTask);
    } else {
        ESP_LOGI(TAG, "未检测到有效配置，启动配网任务");
        task_config_t provTask = {
            .name = "provision",
            .function = taskProvision,
            .stackSize = 8192,
            .priority = 5,
            .param = NULL
        };
        taskManagerCreate(&provTask);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        taskManagerPrintStatus();
    }
}
