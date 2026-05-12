#include "ha_service.h"
#include "ha_mqtt.h"
#include "ha_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

static const char *TAG = "HA_SERVICE";

static TaskHandle_t g_service_task = NULL;
static bool g_service_running = false;

static ai_trigger_callback_t g_ai_trigger_callback = NULL;

void ha_service_set_ai_callback(ai_trigger_callback_t callback) {
    g_ai_trigger_callback = callback;
}

void ha_service_handle_mqtt_data(const char *topic, const char *data, int data_len) {
    if (strncmp(topic, MQTT_TRIGGER_AI_TOPIC, strlen(MQTT_TRIGGER_AI_TOPIC)) != 0) {
        return;
    }

    ESP_LOGI(TAG, "收到AI触发命令: %.*s", data_len, data);

    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON解析失败");
        ha_mqtt_publish(MQTT_TRIGGER_AI_RESULT, "{\"status\":\"failed\",\"message\":\"Invalid JSON\"}", 1, 0);
        return;
    }

    cJSON *action = cJSON_GetObjectItem(root, "action");
    if (action == NULL || !cJSON_IsString(action) || strcmp(action->valuestring, "start") != 0) {
        ESP_LOGE(TAG, "无效的action");
        cJSON_Delete(root);
        ha_mqtt_publish(MQTT_TRIGGER_AI_RESULT, "{\"status\":\"failed\",\"message\":\"Invalid action\"}", 1, 0);
        return;
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "触发AI分析任务");

    if (g_ai_trigger_callback != NULL) {
        g_ai_trigger_callback();
        ha_mqtt_publish(MQTT_TRIGGER_AI_RESULT, "{\"status\":\"success\",\"message\":\"AI task started\"}", 1, 0);
    } else {
        ESP_LOGW(TAG, "AI回调未设置");
        ha_mqtt_publish(MQTT_TRIGGER_AI_RESULT, "{\"status\":\"failed\",\"message\":\"AI callback not set\"}", 1, 0);
    }
}

esp_err_t ha_service_start(void) {
    if (g_service_running) {
        ESP_LOGW(TAG, "服务任务已在运行");
        return ESP_OK;
    }

    g_service_running = true;

    ESP_LOGI(TAG, "服务任务已启动");
    return ESP_OK;
}

esp_err_t ha_service_stop(void) {
    g_service_running = false;
    ESP_LOGI(TAG, "服务任务已停止");
    return ESP_OK;
}
