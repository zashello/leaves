#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "service/ai_service.h"
#include "network/wifi_sta.h"
#include "task_ai.h"

static const char *TAG = "TASK_AI";

void taskAi(void *param)
{
    ESP_LOGI(TAG, "AI分析任务启动");

    if (!wifiStaWaitConnected(30000)) {
        ESP_LOGE(TAG, "WiFi连接超时，AI任务退出");
    } else {
        aiServiceRun();
    }

    vTaskDelete(NULL);
}
