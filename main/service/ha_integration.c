#include <string.h>
#include "esp_log.h"
#include "cJSON.h"
#include "app_config.h"
#include "network/mqtt_wrapper.h"
#include "ha_integration.h"

static const char *TAG = "HA_INT";

static void (*g_ai_trigger_callback)(void) = NULL;
static void (*g_sensor_trigger_callback)(void) = NULL;

static void handleTriggerTopic(const char *data, int dataLen)
{
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

static void handleCommandTopic(const char *data, int dataLen)
{
    ESP_LOGI(TAG, "收到设备命令: %.*s", dataLen, data);

    cJSON *root = cJSON_ParseWithLength(data, dataLen);
    if (root == NULL) {
        ESP_LOGE(TAG, "命令JSON解析失败");
        mqttClientPublish(MQTT_COMMAND_RESULT, "{\"status\":\"failed\",\"message\":\"Invalid JSON\"}", 1, 0);
        return;
    }

    cJSON *action = cJSON_GetObjectItem(root, "action");
    if (action == NULL || !cJSON_IsString(action)) {
        ESP_LOGE(TAG, "缺少action字段");
        cJSON_Delete(root);
        mqttClientPublish(MQTT_COMMAND_RESULT, "{\"status\":\"failed\",\"message\":\"Missing action\"}", 1, 0);
        return;
    }

    const char *actionStr = action->valuestring;

    if (strcmp(actionStr, "ai_analysis") == 0) {
        cJSON_Delete(root);
        ESP_LOGI(TAG, "命令触发AI分析");
        if (g_ai_trigger_callback != NULL) {
            g_ai_trigger_callback();
            mqttClientPublish(MQTT_COMMAND_RESULT, "{\"status\":\"success\",\"message\":\"AI analysis started\"}", 1, 0);
        } else {
            ESP_LOGW(TAG, "AI回调未设置");
            mqttClientPublish(MQTT_COMMAND_RESULT, "{\"status\":\"failed\",\"message\":\"AI callback not set\"}", 1, 0);
        }
    } else if (strcmp(actionStr, "sensor_report") == 0) {
        cJSON_Delete(root);
        ESP_LOGI(TAG, "命令触发传感器立即上报");
        if (g_sensor_trigger_callback != NULL) {
            g_sensor_trigger_callback();
            mqttClientPublish(MQTT_COMMAND_RESULT, "{\"status\":\"success\",\"message\":\"Sensor report triggered\"}", 1, 0);
        } else {
            ESP_LOGW(TAG, "传感器回调未设置");
            mqttClientPublish(MQTT_COMMAND_RESULT, "{\"status\":\"failed\",\"message\":\"Sensor callback not set\"}", 1, 0);
        }
    } else {
        ESP_LOGW(TAG, "未知命令: %s", actionStr);
        cJSON_Delete(root);
        char resp[128];
        snprintf(resp, sizeof(resp), "{\"status\":\"failed\",\"message\":\"Unknown action: %s\"}", actionStr);
        mqttClientPublish(MQTT_COMMAND_RESULT, resp, 1, 0);
    }
}

static void mqttDataCallback(const char *topic, const char *data, int dataLen)
{
    if (strncmp(topic, MQTT_TRIGGER_TOPIC, strlen(MQTT_TRIGGER_TOPIC)) == 0) {
        handleTriggerTopic(data, dataLen);
    } else if (strncmp(topic, MQTT_COMMAND_TOPIC, strlen(MQTT_COMMAND_TOPIC)) == 0) {
        handleCommandTopic(data, dataLen);
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

void haIntegrationSetSensorCallback(void (*callback)(void))
{
    g_sensor_trigger_callback = callback;
}
