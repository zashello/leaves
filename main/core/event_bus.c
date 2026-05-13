#include "event_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "EVENT_BUS";

#define MAX_SUBSCRIBERS 4
#define EVENT_QUEUE_DEPTH 16
#define MAX_EVENT_DATA 64

typedef struct {
    app_event_t event;
    uint8_t data[MAX_EVENT_DATA];
    size_t dataLen;
} event_msg_t;

static event_callback_t g_subscribers[EVENT_AI_COMPLETE + 1][MAX_SUBSCRIBERS];
static int g_sub_counts[EVENT_AI_COMPLETE + 1];
static QueueHandle_t g_event_queue = NULL;
static TaskHandle_t g_dispatch_task = NULL;
static bool g_running = false;

static const char *event_names[] = {
    "WIFI_CONNECTED",
    "WIFI_DISCONNECTED",
    "MQTT_CONNECTED",
    "MQTT_DISCONNECTED",
    "PROVISION_DONE",
    "AI_TRIGGER",
    "AI_COMPLETE",
    "SENSOR_DATA_READY",
    "BUTTON_LONG_PRESS",
};

static void dispatchTask(void *param)
{
    event_msg_t msg;
    while (g_running) {
        if (xQueueReceive(g_event_queue, &msg, pdMS_TO_TICKS(1000)) != pdTRUE) {
            continue;
        }

        int evt = (int)msg.event;
        if (evt < 0 || evt > EVENT_AI_COMPLETE) continue;

        for (int i = 0; i < g_sub_counts[evt]; i++) {
            if (g_subscribers[evt][i]) {
                g_subscribers[evt][i](msg.event, msg.dataLen > 0 ? msg.data : NULL);
            }
        }
    }
    g_dispatch_task = NULL;
    vTaskDelete(NULL);
}

void eventBusInit(void)
{
    memset(g_subscribers, 0, sizeof(g_subscribers));
    memset(g_sub_counts, 0, sizeof(g_sub_counts));

    g_event_queue = xQueueCreate(EVENT_QUEUE_DEPTH, sizeof(event_msg_t));
    if (g_event_queue == NULL) {
        ESP_LOGE(TAG, "事件队列创建失败");
        return;
    }

    g_running = true;
    BaseType_t ret = xTaskCreate(dispatchTask, "event_bus", 4096, NULL, 6, &g_dispatch_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "事件分发任务创建失败");
        g_running = false;
        return;
    }

    ESP_LOGI(TAG, "事件总线初始化完成");
}

void eventBusPublish(app_event_t event, void *data, size_t dataLen)
{
    if (g_event_queue == NULL) return;

    event_msg_t msg = {0};
    msg.event = event;
    msg.dataLen = 0;

    if (data != NULL && dataLen > 0 && dataLen <= MAX_EVENT_DATA) {
        memcpy(msg.data, data, dataLen);
        msg.dataLen = dataLen;
    }

    int evt = (int)event;
    const char *name = (evt >= 0 && evt < sizeof(event_names) / sizeof(event_names[0])) ? event_names[evt] : "UNKNOWN";
    ESP_LOGD(TAG, "发布事件: %s", name);

    if (xQueueSend(g_event_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "事件队列已满，丢弃: %s", name);
    }
}

void eventBusSubscribe(app_event_t event, event_callback_t callback)
{
    if (callback == NULL) return;

    int evt = (int)event;
    if (evt < 0 || evt > EVENT_AI_COMPLETE) return;
    if (g_sub_counts[evt] >= MAX_SUBSCRIBERS) {
        ESP_LOGW(TAG, "事件订阅已满: %d", evt);
        return;
    }

    g_subscribers[evt][g_sub_counts[evt]++] = callback;
    ESP_LOGI(TAG, "订阅事件: %s (%d/%d)", event_names[evt], g_sub_counts[evt], MAX_SUBSCRIBERS);
}
