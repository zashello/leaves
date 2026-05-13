#include <string.h>
#include "esp_log.h"
#include "cJSON.h"
#include "app_config.h"
#include "network/mqtt_wrapper.h"
#include "ha_integration.h"

static const char *TAG = "HA_INT";

static void (*g_ai_trigger_callback)(void) = NULL;

static void mqttDataCallback(const char *topic, const char *data, int dataLen)
{
    if (strncmp(topic, MQTT_TRIGGER_TOPIC, strlen(MQTT_TRIGGER_TOPIC)) != 0) return;

    ESP_LOGI(TAG, "收到AI触发命令: %.*s", dataLen, data);

    cJSON *root = cJSON_ParseWithLength(data, dataLen);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON解析失败");
        mqttClientPublish(MQTT_TRIGGER_RESULT, "{\"status\":\"failed\",\"message\":\"Invalid JSON\"}", 1, 0);
        return;
    }

    cJSON *action = cJSON_GetObjectItem(root, "action");
    if (action == NULL || !cJSON_IsString(action) || strcmp(action->valuestring, "start") != 0) {
        ESP_LOGE(TAG, "无效的action");
        cJSON_Delete(root);
        mqttClientPublish(MQTT_TRIGGER_RESULT, "{\"status\":\"failed\",\"message\":\"Invalid action\"}", 1, 0);
        return;
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "触发AI分析任务");

    if (g_ai_trigger_callback != NULL) {
        g_ai_trigger_callback();
        mqttClientPublish(MQTT_TRIGGER_RESULT, "{\"status\":\"success\",\"message\":\"AI task started\"}", 1, 0);
    } else {
        ESP_LOGW(TAG, "AI回调未设置");
        mqttClientPublish(MQTT_TRIGGER_RESULT, "{\"status\":\"failed\",\"message\":\"AI callback not set\"}", 1, 0);
    }
}

esp_err_t haIntegrationStart(void)
{
    mqttClientSetDataCallback(mqttDataCallback);
    ESP_LOGI(TAG, "HomeAssistant集成启动");
    return ESP_OK;
}

void haIntegrationHandleMqtt(const char *topic, const char *data, int dataLen)
{
    mqttDataCallback(topic, data, dataLen);
}

void haIntegrationSetAiCallback(void (*callback)(void))
{
    g_ai_trigger_callback = callback;
}
