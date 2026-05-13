#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_wrapper.h"
#include "mqtt_client.h"
#include "app_config.h"
#include "storage/storage.h"
#include "core/event_bus.h"

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t g_client = NULL;
static mqtt_state_t g_state = MQTT_STATE_DISCONNECTED;
static TaskHandle_t g_reconnect_task = NULL;
static mqtt_data_callback_t g_data_callback = NULL;

static void mqttReconnectTask(void *param);

static void mqttEventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT已连接");
            g_state = MQTT_STATE_CONNECTED;
            esp_mqtt_client_publish(event->client, MQTT_STATE_TOPIC, MQTT_LWT_ONLINE, 0, 1, 1);
            esp_mqtt_client_subscribe(event->client, MQTT_TRIGGER_TOPIC, 1);
            eventBusPublish(EVENT_MQTT_CONNECTED, NULL, 0);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT已断开连接");
            g_state = MQTT_STATE_DISCONNECTED;
            eventBusPublish(EVENT_MQTT_DISCONNECTED, NULL, 0);
            if (g_reconnect_task == NULL) {
                xTaskCreate(mqttReconnectTask, "mqtt_reconn", 4096, NULL, 5, &g_reconnect_task);
            }
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "已订阅主题: msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "已取消订阅: msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "消息已发布: msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "收到MQTT数据: topic=%.*s, data_len=%d",
                     event->topic_len, event->topic, event->data_len);
            if (g_data_callback && event->topic_len <= 128 && event->data_len <= 512) {
                char topic[129] = {0};
                char data[513] = {0};
                memcpy(topic, event->topic, event->topic_len);
                memcpy(data, event->data, event->data_len);
                g_data_callback(topic, data, event->data_len);
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT错误");
            break;

        default:
            break;
    }
}

static void buildMqttUri(const device_config_t *config, char *uri, size_t uriSize)
{
    if (strlen(config->mqttUsername) > 0) {
        snprintf(uri, uriSize, "mqtt://%s:%s@%s:%d",
                 config->mqttUsername, config->mqttPassword,
                 config->mqttServer, config->mqttPort);
    } else {
        snprintf(uri, uriSize, "mqtt://%s:%d",
                 config->mqttServer, config->mqttPort);
    }
}

static void mqttReconnectTask(void *param)
{
    while (g_state != MQTT_STATE_CONNECTED) {
        if (g_state == MQTT_STATE_CONNECTING) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        g_state = MQTT_STATE_RECONNECTING;
        ESP_LOGI(TAG, "尝试重连MQTT服务器...");

        device_config_t config;
        if (storageLoad(&config) != ESP_OK || strlen(config.mqttServer) == 0) {
            ESP_LOGW(TAG, "MQTT配置无效，停止重连");
            break;
        }

        char uri[300];
        buildMqttUri(&config, uri, sizeof(uri));

        if (g_client == NULL) {
            esp_mqtt_client_config_t mqtt_cfg = {
                .broker.address.uri = uri,
                .session = {
                    .last_will = {
                        .topic = MQTT_STATE_TOPIC,
                        .msg = MQTT_LWT_OFFLINE,
                        .qos = 1,
                        .retain = 1
                    }
                }
            };
            g_client = esp_mqtt_client_init(&mqtt_cfg);
            if (g_client == NULL) {
                ESP_LOGE(TAG, "MQTT客户端初始化失败");
                vTaskDelay(pdMS_TO_TICKS(MQTT_RECONNECT_MS));
                continue;
            }
            esp_mqtt_client_register_event(g_client, MQTT_EVENT_ANY, mqttEventHandler, NULL);
        } else {
            esp_mqtt_client_set_uri(g_client, uri);
        }

        g_state = MQTT_STATE_CONNECTING;
        esp_err_t ret = esp_mqtt_client_start(g_client);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "MQTT客户端启动失败: %s", esp_err_to_name(ret));
            g_state = MQTT_STATE_DISCONNECTED;
        }

        vTaskDelay(pdMS_TO_TICKS(MQTT_RECONNECT_MS));
    }

    g_reconnect_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t mqttClientInit(void)
{
    if (g_client != NULL) {
        ESP_LOGW(TAG, "MQTT客户端已初始化");
        return ESP_OK;
    }

    device_config_t config;
    esp_err_t ret = storageLoad(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "加载配置失败");
        return ret;
    }

    if (strlen(config.mqttServer) == 0) {
        ESP_LOGW(TAG, "MQTT服务器未配置，跳过MQTT初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化MQTT客户端: %s:%d", config.mqttServer, (int)config.mqttPort);

    char uri[300];
    buildMqttUri(&config, uri, sizeof(uri));

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .session = {
            .last_will = {
                .topic = MQTT_STATE_TOPIC,
                .msg = MQTT_LWT_OFFLINE,
                .qos = 1,
                .retain = 1
            }
        }
    };

    g_client = esp_mqtt_client_init(&mqtt_cfg);
    if (g_client == NULL) {
        ESP_LOGE(TAG, "MQTT客户端初始化失败");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(g_client, MQTT_EVENT_ANY, mqttEventHandler, NULL);

    ESP_LOGI(TAG, "MQTT客户端初始化成功");
    return ESP_OK;
}

esp_err_t mqttClientDeinit(void)
{
    if (g_client != NULL) {
        mqttClientPublish(MQTT_STATE_TOPIC, MQTT_LWT_OFFLINE, 1, 1);
        esp_mqtt_client_stop(g_client);
        esp_mqtt_client_destroy(g_client);
        g_client = NULL;
    }
    g_state = MQTT_STATE_DISCONNECTED;
    ESP_LOGI(TAG, "MQTT客户端已清理");
    return ESP_OK;
}

esp_err_t mqttClientConnect(void)
{
    if (g_client == NULL) {
        ESP_LOGE(TAG, "MQTT客户端未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (g_state == MQTT_STATE_CONNECTED || g_state == MQTT_STATE_CONNECTING) {
        ESP_LOGW(TAG, "MQTT已连接或正在连接");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "启动MQTT客户端");
    g_state = MQTT_STATE_CONNECTING;
    esp_err_t ret = esp_mqtt_client_start(g_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MQTT客户端启动失败: %s", esp_err_to_name(ret));
        g_state = MQTT_STATE_DISCONNECTED;
        return ret;
    }

    int retry = 0;
    const int max_retry = MQTT_CONNECT_TIMEOUT / 1000;
    while (g_state != MQTT_STATE_CONNECTED && retry < max_retry) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry++;
    }

    if (g_state != MQTT_STATE_CONNECTED) {
        ESP_LOGE(TAG, "MQTT连接超时");
        esp_mqtt_client_stop(g_client);
        g_state = MQTT_STATE_DISCONNECTED;
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t mqttClientDisconnect(void)
{
    if (g_client == NULL) return ESP_OK;
    mqttClientPublish(MQTT_STATE_TOPIC, MQTT_LWT_OFFLINE, 1, 1);
    esp_mqtt_client_stop(g_client);
    g_state = MQTT_STATE_DISCONNECTED;
    ESP_LOGI(TAG, "MQTT已断开");
    return ESP_OK;
}

bool mqttClientIsConnected(void)
{
    return g_state == MQTT_STATE_CONNECTED;
}

mqtt_state_t mqttClientGetState(void)
{
    return g_state;
}

esp_err_t mqttClientPublish(const char *topic, const char *data, int qos, int retain)
{
    if (g_client == NULL || !mqttClientIsConnected()) {
        ESP_LOGW(TAG, "MQTT未连接，跳过发布: %s", topic);
        return ESP_ERR_INVALID_STATE;
    }

    int msg_id = esp_mqtt_client_publish(g_client, topic, data, strlen(data), qos, retain);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "发布失败: topic=%s", topic);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "发布消息: topic=%s, msg_id=%d", topic, msg_id);
    return ESP_OK;
}

esp_err_t mqttClientSubscribe(const char *topic, int qos)
{
    if (g_client == NULL) return ESP_ERR_INVALID_STATE;
    int msg_id = esp_mqtt_client_subscribe(g_client, topic, qos);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "订阅失败: topic=%s", topic);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "订阅主题: topic=%s, msg_id=%d", topic, msg_id);
    return ESP_OK;
}

void mqttClientSetDataCallback(mqtt_data_callback_t callback)
{
    g_data_callback = callback;
}
