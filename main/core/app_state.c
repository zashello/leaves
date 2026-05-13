#include "app_state.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "APP_STATE";
static app_state_t g_state = APP_STATE_INIT;
static SemaphoreHandle_t g_mutex = NULL;

#define MAX_STATE_CALLBACKS 8
static state_callback_t g_callbacks[MAX_STATE_CALLBACKS];
static int g_callback_count = 0;

static const char *state_names[] = {
    "INIT",
    "PROVISION",
    "CONNECTING",
    "RUNNING",
    "ERROR"
};

void appStateInit(void)
{
    g_mutex = xSemaphoreCreateMutex();
    g_state = APP_STATE_INIT;
    g_callback_count = 0;
    ESP_LOGI(TAG, "状态机初始化完成");
}

void appStateSet(app_state_t state)
{
    if (g_mutex == NULL) return;

    xSemaphoreTake(g_mutex, portMAX_DELAY);
    app_state_t old = g_state;
    g_state = state;
    xSemaphoreGive(g_mutex);

    if (old != state) {
        ESP_LOGI(TAG, "状态变更: %s -> %s", state_names[old], state_names[state]);
        for (int i = 0; i < g_callback_count; i++) {
            if (g_callbacks[i]) {
                g_callbacks[i](old, state);
            }
        }
    }
}

app_state_t appStateGet(void)
{
    if (g_mutex == NULL) return APP_STATE_INIT;
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    app_state_t s = g_state;
    xSemaphoreGive(g_mutex);
    return s;
}

void appStateRegisterCallback(state_callback_t cb)
{
    if (cb == NULL || g_callback_count >= MAX_STATE_CALLBACKS) return;
    g_callbacks[g_callback_count++] = cb;
}

const char* appStateGetName(app_state_t state)
{
    if (state >= 0 && state < sizeof(state_names) / sizeof(state_names[0])) {
        return state_names[state];
    }
    return "UNKNOWN";
}
